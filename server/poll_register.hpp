#ifndef _POLL_REGISTER_H
#define _POLL_REGISTER_H

#include <vector>
#include <functional>

#include <poll.h>

class poll_register {
public:
    using callback_type = std::function<void(short)>;

    struct event {
        int fd;
        short events;
        callback_type callback;
    };

    static poll_register& instance();

    poll_register() = default;
    ~poll_register() = default;

    void register_event(int fd, short events);
    void unregister_event(int fd, short events);
    void unregister_fd(int fd);
    void register_callback(int fd, callback_type cb);
    int poll(std::vector<event>& events);
    int poll_and_dispatch();

private:
    std::vector<struct pollfd> _fds;
    std::vector<callback_type> _callbacks;

    static poll_register global_instance;
};

#endif
