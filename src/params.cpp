#include <iostream>

#include "../include/err.hpp"
#include "../include/params.hpp"

namespace {
  void badParams() {
    syserr("Error while parsing params");
  }

  std::string parsePort(std::string value) {
    unsigned port;
    try {
      port = std::stoul(value);
    } catch (...) {
      port = 0;
    }
    if (port < MIN_PORT || MAX_PORT < port)
      syserr("Invalid port number");
    return value;
  }

  bool parseYesNo(std::string value) {
    if (value == "no")
      return false;
    if (value == "yes")
      return true;
    syserr("Invalid option for -m");
    return false; // unreachable
  }

  int parseTimeout(std::string value) {
    int timeout = 5;
    try {
      timeout = std::stoi(value);
    } catch (...) {
      syserr("Invalid timeout value");
    }
    return timeout;
  }
}

ParamsRadio::ParamsRadio(int argc, char *argv[]) {
  if (argc % 2 == 0)
    badParams();

  // set default values
  this->send_metadata = false;
  this->server_timeout.tv_sec = 5;
  this->server_timeout.tv_usec = 0;
  this->variant_B = false;

  this->proxy_timeout.tv_sec = 5;
  this->proxy_timeout.tv_usec = 0;

  this->hasHost = false;
  this->hasPort = false;
  this->hasResource = false;

  this->hasBPort = false;
  this->hasMulticast = false;

  for (int i = 1; i < argc; i+=2) {
    std::string option(argv[i]);
    std::string value(argv[i+1]);
    if      (option == "-h") {
      this->host = value;
      this->hasHost = true;
    }
    else if (option == "-p") {
      this->server_port = parsePort(value);
      this->hasPort = true;
    }
    else if (option == "-r") {
      this->resource = value;
      this->hasResource = true;
    }
    else if (option == "-m") {
      this->send_metadata = parseYesNo(value);
    }
    else if (option == "-t") {
      this->server_timeout.tv_sec = parseTimeout(value);
    }
    else if (isOptionB(option)) {
      std::cerr << "Part B is not implemented\n";
      exit(2);
    }
    else if (option == "-P") {
      this->proxy_port = parsePort(value);
      this->hasBPort = true;
    }
    else if (option == "-B") {
      this->multicast_addr = value;
      this->hasMulticast = true;
    }
    else if (option == "-T") {
      this->proxy_timeout.tv_sec = parseTimeout(value);
    }
    else {
      badParams();
    }
  }

  if (!hasHost || !hasPort || !hasResource)
    badParams();

  if (variant_B && !hasBPort)
    badParams();
}

bool ParamsRadio::isOptionB(std::string option) {
  if (option == "-P" || option == "-B" || option == "-T")
    return variant_B = true;
  return false;
}

bool ParamsRadio::getSendMetadata() {
  return send_metadata;
}
std::string ParamsRadio::getHost() {
  return host;
}
std::string ParamsRadio::getResource() {
  return resource;
}
std::string ParamsRadio::getProxyPort() {
  return proxy_port;
}
std::string ParamsRadio::getServerPort() {
  return server_port;
}
std::string ParamsRadio::getMulticastAddr() {
  return multicast_addr;
}
struct timeval ParamsRadio::getProxyTimeout() {
  return proxy_timeout;
}
struct timeval ParamsRadio::getServerTimeout() {
  return server_timeout;
}

void ParamsRadio::printAll() {
  std::cout
    << "resource: " << resource << std::endl
    << "host: " << host << std::endl
    << "server_port: " << server_port << std::endl;
    // << "server_timeout: " << server_timeout << std::endl;
  if (variant_B)
  std::cout
    << "proxy_port: " << proxy_port << std::endl
    // << "proxy_timeout: " << proxy_timeout << std::endl
    << "multicast_addr: " << multicast_addr << std::endl;
}

std::string ParamsRadio::getRequest() {
  std::string request = "GET ";
  request += resource;
  request += " HTTP/1.1\r\nHost: ";
  request += host;
  // request += "\r\nUser-Agent: Ecast";
  request += "\r\nIcy-MetaData: ";
  request += (send_metadata ? "1" : "0");
  request += "\r\n\r\n";
  return request;
}