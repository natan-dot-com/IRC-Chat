#include <iostream>
#include <algorithm>

#include "poll_registry.hpp"

poll_registry poll_registry::global_instance;
poll_registry& poll_registry::instance() { return global_instance; }

poll_registry::token_type poll_registry::register_event(int fd, short events, callback_type cb) {
    token_type tok = _next_tok++;
    _fds.emplace_back((struct pollfd) {
        .fd = fd,
        .events = events,
    });
    _callbacks_and_toks.emplace_back(std::move(cb), tok);
    return tok;
}

bool poll_registry::unregister_event(token_type tok) {
    auto it = std::find_if(_callbacks_and_toks.cbegin(), _callbacks_and_toks.cend(),
                           [=](auto& cb_tok){ return cb_tok.second == tok; });
    if (it == _callbacks_and_toks.cend()) return false;
    _callbacks_and_toks.erase(it);

    // pollfd will be in the same index
    _fds.erase(_fds.cbegin() + std::distance(_callbacks_and_toks.cbegin(), it));
    return true;
}

int poll_registry::poll(std::vector<token_type>& events) {
    int n_events = ::poll(_fds.data(), _fds.size(), -1);
    if (n_events < 0) return n_events;
    events.reserve(n_events);
    for (size_t i = 0; i < _fds.size(); i++) {
        if (_fds[i].revents)
            events.push_back(_callbacks_and_toks[i].second);
    }
    return n_events;
}

int poll_registry::poll_and_dispatch() {
    int n_events = ::poll(_fds.data(), _fds.size(), -1);
    if (n_events < 0) return n_events;
    std::cout << "NOTIFIED" << std::endl;

    for (size_t i = 0; i < _fds.size(); i++) {
        auto& pollfd = _fds[i];
        auto& [cb, _] = _callbacks_and_toks[i];
        if (pollfd.revents & pollfd.events) cb(pollfd.revents);
    }
    return n_events;
}
