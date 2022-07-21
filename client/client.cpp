#include <sys/types.h>
#include <sys/socket.h>

#include "client.hpp"
#include "config.hpp"

client::client() {
    _fd = socket(AF_INET, SOCK_STREAM, 0);
}

int client::connect(int serverPort) {
    struct sockaddr_in remote = {0};

    remote.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    remote.sin_family = AF_INET;
    remote.sin_port = htons(serverPort);

    return ::connect(_fd, (struct sockaddr *)& remote, sizeof(struct sockaddr_in)) != 0;
}

int client::send(std::string msg) {
    return ::send(_fd, msg.data(), msg.size(), 0);
}

int client::recv(std::string& rsp) {
    std::vector<uint8_t> writable(MAX_SIZE);

    int nrecvd = ::recv(_fd, writable.data(), MAX_SIZE, 0);
    rsp = std::string(writable.cbegin(), writable.cbegin() + nrecvd);
    return nrecvd;
}
