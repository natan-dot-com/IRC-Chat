#include <sys/eventfd.h>
#include <unistd.h>

#include "message_queue.hpp"
#include "utils.hpp"


namespace irc {

    /* implementations for `message` */

    message::message(std::optional<std::string> prefix, enum command command,
                     std::vector<std::string> params)
        : prefix(prefix)
        , command(command)
        , params(params)
    { }

    message::message(enum command command, std::vector<std::string> params)
        : command(command)
        , params(params)
    { }

    /* implementations for `message_queue` */

    message_queue::message_queue() {
        _ev_fd = eventfd(0, EFD_NONBLOCK);
        if (_ev_fd < 0) THROW_ERRNO("eventfd failed");
    }

    void message_queue::send_message(message message) {
        uint64_t val = 1;
        auto ret = ::write(_ev_fd, &val, sizeof(val));
        if (ret != sizeof(val)) THROW_ERRNO("writing to the eventfd failed");
        _messages.emplace_back(std::move(message));
    }

    size_t message_queue::size() const { return _messages.size(); }

    message_queue::const_iterator message_queue::cbegin() const {
        return message_queue::const_iterator(_messages, 0);
    }

    message_queue::const_iterator message_queue::cend() const {
        return message_queue::const_iterator(_messages, _messages.size());
    }

    int message_queue::fd() const { return _ev_fd; }

    /* implementations for `message_queue::const_iterator` */

    message_queue::const_iterator::const_iterator(const message_queue::queue_type& queue, size_t idx)
        : _queue(queue)
        , _idx(idx)
    { }

    message_queue::const_iterator::const_iterator(const message_queue::const_iterator& rhs)
        : _queue(rhs._queue)
        , _idx(rhs._idx)
    { }

    bool message_queue::const_iterator::operator<(const const_iterator& rhs ) const {
        return _idx <  rhs._idx;
    }

    bool message_queue::const_iterator::operator==(const const_iterator& rhs) const {
        return _idx == rhs._idx;
    }

    bool message_queue::const_iterator::operator!=(const const_iterator& rhs) const {
        return _idx != rhs._idx;
    }

    message_queue::const_iterator message_queue::const_iterator::operator=(const const_iterator& rhs) {
        _idx = rhs._idx;
        _queue = rhs._queue;
        return *this;
    }

    const message_queue::entry& message_queue::const_iterator::operator*() {
        return queue()[_idx];
    }

    const message_queue::entry* message_queue::const_iterator::operator->() {
        return &queue()[_idx];
    }

    message_queue::const_iterator message_queue::const_iterator::operator++() {
        ++_idx;
        return *this;
    }

    message_queue::const_iterator message_queue::const_iterator::operator++(int) {
        auto copy = *this;
        ++_idx;
        return copy;
    }

    const message_queue::queue_type& message_queue::const_iterator::queue() const {
        return _queue.get();
    }
}
