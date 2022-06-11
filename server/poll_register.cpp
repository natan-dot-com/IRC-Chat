#include <algorithm>

#include "poll_register.hpp"

poll_register poll_register::global_instance;
poll_register& poll_register::instance() { return global_instance; }

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
    auto it = std::find_if(_fds.begin(), _fds.end(),
                          [&](auto& pollfd){return pollfd.fd == fd;});

    if (it != _fds.end()) _fds.erase(it);
}

void poll_register::register_callback(int fd, callback_type cb) {
    if (fd >= _callbacks.size()) _callbacks.resize(fd + 1);
    _callbacks[fd] = std::move(cb);
}

int poll_register::poll(std::vector<event>& events) {
    int n_events = ::poll(_fds.data(), _fds.size(), -1);
    if (n_events < 0) return n_events;
    events.reserve(n_events);
    for (auto& pollfd : _fds) {
        if (pollfd.revents)
            events.push_back((event){
                .fd = pollfd.fd,
                .events = pollfd.revents,
                .callback = pollfd.fd < _callbacks.size()
                                ? _callbacks[pollfd.fd]
                                : callback_type()
            });
    }
    return n_events;
}

int poll_register::poll_and_dispatch() {
    std::vector<event> events;
    int res = poll(events);
    if (res < 0) return res;

    for (const auto& ev : events) {
        if (ev.callback)
            ev.callback(ev.events);
    }
    return res;
}
