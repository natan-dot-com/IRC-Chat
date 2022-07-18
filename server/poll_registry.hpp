#ifndef _POLL_REGISTER_H
#define _POLL_REGISTER_H

#include <vector>
#include <utility>
#include <functional>

#include <poll.h>

class poll_registry {
public:
    using callback_type = std::function<void(short)>;
    using token_type = size_t;

    static poll_registry& instance();

    poll_registry() = default;
    ~poll_registry() = default;

    // Register to wait for `events` in file descriptor `fd` that, when notified, should call `cb`.
    // This function returns a token which can be used to unregister this listener. This is
    // necessary because it is possible to register multiple listeners for the same file
    // descriptor.
    token_type register_event(int fd, short events, callback_type cb);
    bool unregister_event(token_type tok);
    int poll(std::vector<token_type>& events);
    int poll_and_dispatch();

private:
    token_type _next_tok = 0;
    std::vector<struct pollfd> _fds;
    std::vector<std::pair<callback_type, token_type>> _callbacks_and_toks;

    static poll_registry global_instance;
};

#endif
