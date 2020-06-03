#ifndef PARAMS_HPP
#define PARAMS_HPP

const unsigned MIN_PORT = 1;
const unsigned MAX_PORT = (1 << 16) - 1;

class ParamsRadio {
public:
  ParamsRadio(int argc, char *argv[]);

  bool getSendMetadata();
  std::string getHost();
  std::string getResource();
  std::string getProxyPort();
  std::string getServerPort();
  std::string getMulticastAddr();
  struct timeval getProxyTimeout();
  struct timeval getServerTimeout();

  std::string getRequest();

private:
  bool send_metadata;
  std::string host;
  std::string resource;
  std::string proxy_port;
  std::string server_port;
  std::string multicast_addr;
  struct timeval proxy_timeout;
  struct timeval server_timeout;

  bool hasHost;
  bool hasPort;
  bool hasResource;

  bool hasBPort;
  bool hasMulticast;
};

#endif /* PARAMS_HPP */