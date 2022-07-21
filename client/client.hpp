#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <optional>
#include <string>
#include <string_view>
#include <cstdint>

#include <sys/socket.h>
#include <arpa/inet.h>

class client {
public:
    client();
    ~client();

    int connect(const char *server, int server_port);
    int send(std::string msg);
    int recv(std::string& rsp);

private:
    int _fd;
};

#endif
