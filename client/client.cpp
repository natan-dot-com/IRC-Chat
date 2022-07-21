#include <array>

#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "client.hpp"
#include "config.hpp"
#include "../common/utils.hpp"

client::client() {
    _fd = socket(AF_INET, SOCK_STREAM, 0);
}

client::~client() {
    ::close(_fd);
}

int client::connect(const char *server, int server_port) {
    struct sockaddr_in remote = {0};

    remote.sin_addr.s_addr = inet_addr(server);
    remote.sin_family = AF_INET;
    remote.sin_port = htons(server_port);

    return ::connect(_fd, (struct sockaddr *)& remote, sizeof(struct sockaddr_in)) != 0;
}

int client::send(std::string msg) {
    return ::send(_fd, msg.data(), msg.size(), 0);
}

int client::recv(std::string& rsp) {
    std::array<uint8_t, MAX_SIZE> writable;

    int nrecvd = ::recv(_fd, writable.data(), MAX_SIZE, 0);
    if (nrecvd < 0) THROW_ERRNO("recv failed");
    rsp = std::string(writable.cbegin(), writable.cbegin() + nrecvd);
    return nrecvd;
}
