#include <cstring>
#include <iostream>
#include <netdb.h>
#include <signal.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>

#include "../include/params.hpp"

#define BUFF_SIZE 8096
char buff[BUFF_SIZE];
size_t last_read;
size_t buff_len;
static bool int_flag = false;

static void setInteruptFlag(int sig) {
  int_flag = true;
}

void print_buff(size_t beg, size_t end, FILE *where) {
  fwrite(buff + beg, end - beg, 1, where);
}

size_t readSock(FILE *sock) {
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
      std::cout << line;
      found = line.find(metaint);
      if (line == "\r\n") return 0;
    }
    interval = stoi(line.substr(metaint.size()));
  }

  while (line != "\r\n") {
    if (!s_getline(fp, line))
      return 1;
  }
  last_read = BUFF_SIZE;
  return interval;
}

void readAndPrint(int length, FILE *from, FILE *where) {
  while (length > 0) {
    if (last_read >= buff_len)
       readSock(from);
    if (buff_len == 0) {
      int_flag = true;
      return;
    }

    size_t end = std::min(last_read + length, buff_len);
    length -= (end - last_read);
    print_buff(last_read, end, where);
    last_read = end;
  }
}

void readRadio(int length, FILE *from) {
  if (length == 0) {
    if (readSock(from) <= 0)
      int_flag = true;
    else
      print_buff(last_read, buff_len, stdout);
    return;
  }
  readAndPrint(length, from, stdout);
}

void readMeta(FILE *fp) {
  if (last_read >= BUFF_SIZE)
    readSock(fp);
  int length = 16 * (int)buff[last_read++];

  readAndPrint(length, fp, stderr);
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

  // If we didn't request metadata we'll skip it
  if (!params.getSendMetadata())
    icy_metaint = 0;

  // Start reading
  bool meta_turn = false;
  while (!int_flag) {
    if (meta_turn && params.getSendMetadata())
      readMeta(serv_d);
    else
      readRadio(icy_metaint, serv_d);
    meta_turn = !meta_turn;
  }

  // Close connection
  fclose(serv_d);
}