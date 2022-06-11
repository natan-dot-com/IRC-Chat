#include "message_queue.hpp"

const message_queue::queue_type& message_queue::const_iterator::queue() const {
    return _queue.get();
}

void message_queue::push_back(size_t id, std::string s) {
    _messages.push_back((entry) {
        .sender_id = id,
        .content = std::move(s),
    });
}

size_t message_queue::size() const { return _messages.size(); }

message_queue::const_iterator message_queue::cbegin() const {
    return message_queue::const_iterator(_messages, 0);
}

message_queue::const_iterator message_queue::cend() const {
    return message_queue::const_iterator(_messages, _messages.size());
}

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

message_queue::const_iterator::const_iterator(const message_queue::queue_type& queue, size_t idx)
    : _queue(queue)
    , _idx(idx)
{ }
