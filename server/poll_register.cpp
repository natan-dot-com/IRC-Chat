#include "poll_register.hpp"

void poll_register::register_event(int fd, short events) {
    auto it = std::find_if(_fds.begin(), _fds.end(),
                           [&](auto& pollfd){return pollfd.fd == fd;});

    if (it != _fds.end()) {
        it->events |= events;
    } else {
        _fds.push_back((struct pollfd) {
            .fd = fd,
            .events = events,
        });
    }
}

void poll_register::unregister_event(int fd, short events) {
    auto it = std::find_if(_fds.begin(), _fds.end(),
                           [&](auto& pollfd){return pollfd.fd == fd;});

    if (it != _fds.end()) {
        it->events &= ~events;
        if (!it->events) _fds.erase(it);
    }
}

void poll_register::unregister_fd(int fd) {
    _fds.erase(
        std::find_if(_fds.begin(), _fds.end(),
                     [&](auto& pollfd){return pollfd.fd == fd;})
    );
}

int poll_register::poll(std::vector<poll_ev>& events) {
    int n_events = ::poll(_fds.data(), _fds.size(), -1);
    if (n_events < 0) return n_events;
    events.reserve(n_events);
    for (auto& pollfd : _fds) {
        if (pollfd.revents)
            events.push_back((poll_ev){
                .fd = pollfd.fd,
                .events = pollfd.revents
            });
    }
    return n_events;
}
