#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "tcpstream.hpp"
#include "utils.hpp"

tcpstream::tcpstream(tcpstream&& rhs) : _fd(rhs._fd) { rhs._fd = -1; }

tcpstream::~tcpstream() { close(); }

tcpstream& tcpstream::operator=(tcpstream&& rhs) {
    close();
    std::swap(_fd, rhs._fd);
    return *this;
}

tcpstream tcpstream::duplicate() {
    // Duplicates the file descriptor and sets the close-on-exec flag.
    int fd = fcntl(_fd, F_DUPFD_CLOEXEC, 0);
    if (fd < 0) throw std::runtime_error("failed to duplicate file descriptor");
    return tcpstream(fd);
}

ssize_t tcpstream::recv(uint8_t* buf, size_t len) {
    return ::recv(_fd, buf, len, 0);
}

ssize_t tcpstream::nonblocking_recv(uint8_t* buf, size_t len) {
    return ::recv(_fd, buf, len, MSG_DONTWAIT);
}

ssize_t tcpstream::send(const uint8_t* buf, size_t len) {
    return ::send(_fd, buf, len, 0);
}

ssize_t tcpstream::nonblocking_send(const uint8_t* buf, size_t len) {
    return ::send(_fd, buf, len, MSG_DONTWAIT);
}

int tcpstream::fd() const { return _fd; }
void tcpstream::close() { if (_fd > 0) ::close(_fd); }

tcpstream tcpstream::connect(const char *ip, uint16_t server_port) {
    struct sockaddr_in remote = {0};

    remote.sin_addr.s_addr = inet_addr(ip);
    remote.sin_family = AF_INET;
    remote.sin_port = htons(server_port);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) THROW_ERRNO("socket failed");
    int ret = ::connect(fd, (struct sockaddr *)& remote, sizeof(struct sockaddr_in)) != 0;
    if (ret < 0) THROW_ERRNO("connect failed");
    if (ret != 0) throw std::runtime_error("Unexpected error");

    return tcpstream(fd);
}
