#ifndef _POLL_REGISTER_H
#define _POLL_REGISTER_H

#include <algorithm>

#include <poll.h>

struct poll_ev {
    int fd;
    short events;
};

class poll_register {
public:
    poll_register() = default;
    ~poll_register() = default;

    void register_event(int fd, short events);
    void unregister_event(int fd, short events);
    void unregister_fd(int fd);
    int poll(std::vector<poll_ev>& events);

private:
    std::vector<struct pollfd> _fds;
};

#endif
