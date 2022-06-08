#include "tcpstream.hpp"

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
