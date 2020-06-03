#include <iostream>

#include "../include/params.hpp"

namespace {
  std::string parsePort(std::string value) {
    unsigned port;
    try {
      port = std::stoul(value);
    } catch (...) {
      port = 0;
    }
    if (port < MIN_PORT || MAX_PORT < port)
      exit(1);
    return value;
  }

  bool parseYesNo(std::string value) {
    if (value == "yes")
      return true;
    return false; // every other options are treated as a "no"
  }

  int parseTimeout(std::string value) {
    int timeout = 5;
    try {
      timeout = std::stoi(value);
    } catch (...) {
      exit(1);
    }
    return timeout;
  }
}

ParamsRadio::ParamsRadio(int argc, char *argv[]) {
  if (argc % 2 == 0)
    exit(1);

  // set default values
  this->send_metadata = false;
  this->server_timeout.tv_sec = 5;
  this->server_timeout.tv_usec = 0;

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
    else if (option == "-P") {
      exit(2);
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
  }

  if (!hasHost || !hasPort || !hasResource)
    exit(1);
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

std::string ParamsRadio::getRequest() {
  std::string request = "GET ";
  request += resource;
  request += " HTTP/1.1\r\nHost: ";
  request += host;
  request += "\r\nIcy-MetaData: ";
  request += (send_metadata ? "1" : "0");
  request += "\r\n\r\n";
  return request;
}