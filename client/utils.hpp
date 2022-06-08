#ifndef _UTILS_H_
#define _UTILS_H_

#include <bits/stdc++.h>
#include <ncurses.h>
#include <atomic>
#include <mutex>
#include "client.hpp"

#define LOCAL_HOST "127.0.0.1"
#define MAX_SIZE 4096

std::string readLine(WINDOW* w);
void sendMessage(WINDOW* w, Client& newClient);
void recvMessage(WINDOW* w, Client& newClient);

#endif
