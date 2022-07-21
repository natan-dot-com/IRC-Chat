#include <cstring>

#include <sys/socket.h>
#include <netinet/in.h>

#include "tcpstream.hpp"
#include "tcplistener.hpp"
#include "utils.hpp"


tcplistener::tcplistener(uint16_t port)
    : _port(port)
    , _init(false)
{
}

tcplistener::tcplistener(tcplistener&& rhs)
    : _port(rhs._port)
    , _init(rhs._init)
    , _fd(rhs._init)
    , _address(rhs._address)
{
    rhs._init = false;
}

tcplistener::~tcplistener() {
    if (_init) {
        shutdown(_fd, SHUT_RDWR);
        close(_fd);
    }
}

void tcplistener::start() {
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

tcpstream tcplistener::accept() {
    assert_init();
    size_t addrlen = sizeof(_address);
    int fd = ::accept(_fd, (struct sockaddr*)&_address, (socklen_t*)&addrlen);
    if (fd < 0) THROW_ERRNO("accept failed");
    return tcpstream(fd);
}

int tcplistener::fd() const {
    assert_init();
    return _fd;
}

void tcplistener::assert_init() const {
    if (!_init) throw std::runtime_error("server not initialized");
}
