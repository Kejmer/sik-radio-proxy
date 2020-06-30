#include <cstring>
#include <iostream>
#include <netdb.h>
#include <signal.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <thread>
#include <netinet/in.h>
#include <mutex>
#include <map>
#include <sys/time.h>
#include <poll.h>


#include "../include/params.hpp"

#define DISCOVER 1
#define IAM 2
#define KEEPALIVE 3
#define AUDIO 4
#define META 6

#define BUFF_SIZE 8096
#define HEADER_SIZE 4

char header[HEADER_SIZE];
char buff_udp[HEADER_SIZE];
char buff[BUFF_SIZE];
size_t last_read_udp;
size_t last_read;
size_t buff_len_udp;
size_t buff_len;

bool radio_b;
int sock_udp;
std::string radio_name = "";

std::mutex client_map_mutex;

static bool int_flag = false;
static bool thread_failed = false;

static void setInteruptFlag(int sig) {
  (void) sig;
  int_flag = true;
}

struct comp
{
  template<typename T>
  bool operator()(const T &lhs, const T &rhs) const
  {
    if (lhs.sin_port != rhs.sin_port)
      return lhs.sin_port > rhs.sin_port;
    return lhs.sin_addr.s_addr > rhs.sin_addr.s_addr;
  }
};
std::map<struct sockaddr_in, struct timeval, comp> client_map;

// Handling CLIENT --- PROXY requests

short parseHeader() {
  short type;
  memcpy(&type, buff_udp, 2);
  last_read_udp += 4;
  return ntohs(type);
}

void createHeader(unsigned short type, unsigned short length, char* c) {
  type = htons(type);
  length = htons(length);
  memcpy(c, &type, 2);
  memcpy(c + 2, &length, 2);
}

void refresh_time(struct sockaddr_in client_address) {
  struct timeval tv;
  gettimeofday(&tv, 0);
  client_map[client_address] = tv;
}

void proxy_iam() {
  createHeader(IAM, (short)radio_name.size(), buff_udp);
}

void proxy_radio(size_t beg, size_t end, short type, int timeout) {
  createHeader(type, end-beg, header);
  size_t snda_len = (socklen_t) sizeof(struct sockaddr_in);
  int flags = 0;
  std::map<struct sockaddr_in, struct timeval>::iterator it, next_it;
  struct timeval now;
  gettimeofday(&now, 0);

  size_t msg_len = HEADER_SIZE + end - beg;
  char msg[msg_len];
  for (size_t i = 0; i < HEADER_SIZE; i++)
    msg[i] = header[i];
  for (size_t i = HEADER_SIZE; i < msg_len; i++)
    msg[i] = buff[beg + i - HEADER_SIZE];

  client_map_mutex.lock();
  for (it = client_map.begin(), next_it = it; it != client_map.end(); it = next_it) {
    next_it++;
    if (it->second.tv_sec + timeout < now.tv_sec ||
       (it->second.tv_sec + timeout == now.tv_sec && it->second.tv_usec >= now.tv_usec)) {
      client_map.erase(it);
    } else {
      sendto(sock_udp, msg, (size_t) msg_len, flags,
              (struct sockaddr *) &it->first, snda_len);
    }
  }
  client_map_mutex.unlock();
}

void proxy_audio(size_t beg, size_t end, int timeout) {
  proxy_radio(beg, end, AUDIO, timeout);
}

void proxy_meta(size_t beg, size_t end, int timeout) {
  proxy_radio(beg, end, META, timeout);
}

void handle_proxy(int port) {
  struct sockaddr_in server_address;
  struct sockaddr_in client_address;
  socklen_t snda_len, rcva_len;

  sock_udp = socket(AF_INET, SOCK_DGRAM, 0); // creating IPv4 UDP socket
  if (sock_udp < 0) {
    int_flag = true;
    thread_failed = true;
    return;
  }

  server_address.sin_family = AF_INET; // IPv4
  server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
  server_address.sin_port = htons(port); // default port for receiving is PORT_NUM

  if (bind(sock_udp, (struct sockaddr *) &server_address,
          (socklen_t) sizeof(server_address)) < 0) {
    int_flag = true;
    thread_failed = true;
    return;
  }

  int flags = 0;
  short type;
  size_t len;

  proxy_iam();
  size_t msg_len = radio_name.size() + HEADER_SIZE;
  char msg[msg_len + 1];
  for (size_t i = 0; i < HEADER_SIZE; i++)
    msg[i] = buff_udp[i];
  for (size_t i = HEADER_SIZE; i < msg_len; i++)
    msg[i] = radio_name[i - HEADER_SIZE];
  msg[msg_len] = 0;

  snda_len = (socklen_t) sizeof(client_address);

  struct pollfd udp_poll;
  int res;
  udp_poll.fd = sock_udp;
  udp_poll.events = POLLIN;
  while (!int_flag) {
    res = poll(&udp_poll, 1, 2000); // 10 s timeout
    if (res <= 0) {
      len = 0;
    } else {
      len = recvfrom(sock_udp, buff_udp, HEADER_SIZE, flags,
          (struct sockaddr *) &client_address, &rcva_len);
    }
    if (len < 4) {
      //error while reading socket or incomplete msg, skipping
    } else {
      type = parseHeader();

      client_map_mutex.lock();
      switch(type) {
      case DISCOVER:
        sendto(sock_udp, msg, (size_t) msg_len, flags,
            (struct sockaddr *) &client_address, snda_len);
        refresh_time(client_address);
        break;
      case KEEPALIVE:
        refresh_time(client_address);
      }
      client_map_mutex.unlock();
    }
  }
}

// Handling PROXY --- SERVER connection

void print_buff(size_t beg, size_t end, FILE *where, int timeout) {
  if (radio_b) {
    if (where == stdout) {
      proxy_audio(beg, end, timeout);
    } else {
      proxy_meta(beg, end, timeout);
    }
  } else {
    fwrite(buff + beg, end - beg, 1, where);
  }
}

size_t readSockTcp(FILE *sock) {
  memset(buff, 0, sizeof(buff));
  buff_len = fread(buff, 1, sizeof(buff), sock);
  last_read = 0;
  if (buff_len == 0)
    int_flag = true;
  return buff_len;
}

// Helper function to read header line by line
bool s_getline(FILE *fp, std::string &str) {
  char *cline = NULL;
  size_t len = BUFF_SIZE;
  if (getline(&cline, &len, fp) == 0) {
    int_flag = true;
    return false;
  }
  str = std::string(cline);
  free(cline);
  return true;
}

// Return 0 if status is not 200 and move fp to the end of header
size_t readHeader(FILE *fp, bool get_meta_int) {
  size_t interval = 1;
  std::string line;
  std::string metaint = "icy-metaint:";
  std::string metaname = "icy-name:";

  if (!s_getline(fp, line))
    return 1;
  size_t found = line.find("200");
  if (found == std::string::npos)
    return 0;

  if (get_meta_int) {
    found = std::string::npos;
    while (found == std::string::npos) {
      if (!s_getline(fp, line))
        return 1;
      std::transform(line.begin(), line.end(), line.begin(),
        [](unsigned char c){ return std::tolower(c); });
      found = line.find(metaname);
      if (found != std::string::npos)
        radio_name = line.substr(metaname.size());
      found = line.find(metaint);

      if (line == "\r\n") return 0;
    }
    interval = stoi(line.substr(metaint.size()));
  }

  while (line != "\r\n") {
    found = line.find(metaname);
    if (found != std::string::npos)
      radio_name = line.substr(metaname.size());
    if (!s_getline(fp, line))
      return 1;
  }
  last_read = BUFF_SIZE;
  return interval;
}

void readAndPrint(int length, FILE *from, FILE *where, int timeout) {
  while (length > 0) {
    if (last_read >= buff_len)
       readSockTcp(from);
    if (buff_len == 0) {
      int_flag = true;
      return;
    }

    size_t end = std::min(last_read + length, buff_len);
    length -= (end - last_read);
    print_buff(last_read, end, where, timeout);
    last_read = end;
  }
}

void readRadio(int length, FILE *from, int timeout) {
  if (length == 0) {
    if (readSockTcp(from) <= 0)
      int_flag = true;
    else
      print_buff(last_read, buff_len, stdout, timeout);
    return;
  }
  readAndPrint(length, from, stdout, timeout);
}

void readMeta(FILE *fp, int timeout) {
  if (last_read >= BUFF_SIZE)
    readSockTcp(fp);
  int length = 16 * (int)buff[last_read++];

  readAndPrint(length, fp, stderr, timeout);
}
int main(int argc, char *argv[]) {
  struct sigaction action;
  sigset_t block_mask;

  // SIGINT handling
  sigemptyset (&block_mask);
  action.sa_handler = setInteruptFlag;
  action.sa_mask = block_mask;
  action.sa_flags = SA_RESTART;

  if (sigaction (SIGINT, &action, 0) == -1)
    exit(1);

  // Params handling
  ParamsRadio params = ParamsRadio(argc, argv);
  radio_b = params.getVariantB();

  // Setup connection info
  struct addrinfo addr_hints;
  struct addrinfo *addr_result;

  memset(&addr_hints, 0, sizeof(struct addrinfo));
  addr_hints.ai_family = AF_INET;
  addr_hints.ai_socktype = SOCK_STREAM;
  addr_hints.ai_protocol = IPPROTO_TCP;
  std::string host = params.getHost();
  std::string port = params.getServerPort();
  if (getaddrinfo(host.c_str(), port.c_str(), &addr_hints, &addr_result) != 0)
    exit(1);

  // Socket
  int sock_tcp = socket(addr_result->ai_family, addr_result->ai_socktype, addr_result->ai_protocol);
  if (sock_tcp < 0)
    exit(1);

  // Timeout setup
  struct timeval timeout = params.getServerTimeout();
  if (setsockopt(sock_tcp, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeout, sizeof(timeout)) < 0) {
    close(sock_tcp);
    exit(1);
  }

  // Connect to server
  if (connect(sock_tcp, addr_result->ai_addr, addr_result->ai_addrlen) < 0) {
    close(sock_tcp);
    exit(1);
  }
  freeaddrinfo(addr_result);

  // Send request to radio provider
  std::string request_s = params.getRequest();
  const char *request = request_s.c_str();
  if (write(sock_tcp, request, strlen(request)) < 0) {
    close(sock_tcp);
    exit(1);
  }

  FILE *serv_d = fdopen(sock_tcp, "r"); // we'll use socket as file descriptor

  // Get Metadata interval
  size_t icy_metaint = readHeader(serv_d, params.getSendMetadata());
  if (icy_metaint == 0) {
    fclose(serv_d);
    exit(1);
  }

  std::thread proxy_connections;

  if (radio_b) {
    proxy_connections = std::thread(handle_proxy, stoi(params.getProxyPort()));
  }

  // If we didn't request metadata we'll skip it
  if (!params.getSendMetadata())
    icy_metaint = 0;

  // Start reading
  bool meta_turn = false;
  while (!int_flag) {
    if (meta_turn && params.getSendMetadata())
      readMeta(serv_d, params.getProxyTimeout().tv_sec);
    else
      readRadio(icy_metaint, serv_d, params.getProxyTimeout().tv_sec);
    meta_turn = !meta_turn;
  }

  if (radio_b) {
    proxy_connections.join();
    close(sock_udp);
  }

  if (thread_failed)
    exit(1);

  // Close connection
  fclose(serv_d);
}