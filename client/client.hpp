#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <bits/stdc++.h>
#include <sys/socket.h>
#include <arpa/inet.h>

class client {
public:

    client();

    int connect(int serverPort);
    int send(std::string msg);
    int recv(std::string& rsp);

private:
    int _fd;
};

#endif
