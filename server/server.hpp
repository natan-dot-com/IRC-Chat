#ifndef _SERVER_H_
#define _SERVER_H_

#include <cstdint>

#include <netinet/in.h>

#include "tcpstream.hpp"

class server {
public:
    server(uint16_t port);
    server(const server&) = delete;
    server(server&& rhs);
    ~server();

    void start();
    tcpstream accept();

    int fd() const;

private:
    uint16_t _port;
    int _fd;
    struct sockaddr_in _address;
    bool _init;
};

#endif
