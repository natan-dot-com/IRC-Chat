#ifndef _TCPSTREAM_H
#define _TCPSTREAM_H

#include <stdexcept>
#include <cstdint>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>

class tcpstream {
public:
    static tcpstream connect(const char *ip, uint16_t port);

    tcpstream(tcpstream&& rhs);
    tcpstream(const tcpstream&) = delete;
    ~tcpstream();

    tcpstream& operator=(tcpstream&& rhs);
    tcpstream duplicate();

    ssize_t recv(uint8_t* buf, size_t len);

    ssize_t nonblocking_recv(uint8_t* buf, size_t len);

    ssize_t send(const uint8_t* buf, size_t len);

    ssize_t nonblocking_send(const uint8_t* buf, size_t len);

    int fd() const;
    void close();

private:
    tcpstream(int fd) : _fd(fd) {}

    friend class tcplistener;

    int _fd;
};

#endif
