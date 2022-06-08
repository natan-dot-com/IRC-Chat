#include <cstring>

#include <sys/socket.h>
#include <netinet/in.h>

#include "tcpstream.hpp"
#include "server.hpp"
#include "utils.hpp"


server::server(uint16_t port)
    : _port(port)
    , _init(false)
{ }

server::server(server&& rhs)
    : _port(rhs._port)
    , _init(rhs._init)
    , _fd(rhs._init)
    , _address(rhs._address)
{
    rhs._init = false;
}

server::~server() { if (_init) shutdown(_fd, SHUT_RDWR); }

void server::start() {
    _fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_fd == 0) THROW_ERRNO("socket failed");

    _address.sin_family = AF_INET;
    _address.sin_addr.s_addr = INADDR_ANY;
    _address.sin_port = htons(_port);

    int opt = 1;
    if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
        THROW_ERRNO("setsockopt failed");

    if (bind(_fd, (struct sockaddr*)&_address, sizeof(_address)) < 0)
        THROW_ERRNO("bind failed");

    if (listen(_fd, 3) < 0)
        THROW_ERRNO("listen failed");

    _init = true;
}

tcpstream server::accept() {
    if (!_init) THROW_ERRNO("server not initialized");
    size_t addrlen = sizeof(_address);
    int fd = ::accept(_fd, (struct sockaddr*)&_address, (socklen_t*)&addrlen);
    if (fd < 0) THROW_ERRNO("accept failed");
    return tcpstream(fd);
}

int server::fd() const { return _fd; }
