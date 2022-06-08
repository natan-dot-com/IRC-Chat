#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <bits/stdc++.h>
#include <sys/socket.h>
#include <arpa/inet.h>

class Client {
    public:
        short id;

        Client();

        int s_connect(int serverPort);
        int s_send(std::string msg);
        int s_recv(std::string *rsp);
};

#endif