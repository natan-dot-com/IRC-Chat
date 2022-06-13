#ifndef _SERVER_H_
#define _SERVER_H_

#include <cstdint>

#include <netinet/in.h>

#include "tcpstream.hpp"

class tcplistener {
public:
    tcplistener(uint16_t port);
    tcplistener(const tcplistener&) = delete;
    tcplistener(tcplistener&& rhs);
    ~tcplistener();

    tcpstream accept();

    int fd() const;

private:
    void start();

    uint16_t _port;
    int _fd;
    struct sockaddr_in _address;
    bool _init;
};

#endif
