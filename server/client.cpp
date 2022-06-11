#include <iomanip>
#include <iostream>

#include "tcpstream.hpp"
#include "client.hpp"
#include "utils.hpp"
#include "poll_register.hpp"

void client::register_callback() {
    // WARNING: This closure carries a reference to `this` which may not live as
    // long as the static reference `poll_register::instance()`. Meaning if
    // `this` is moved, we MUST also update the closure with a new reference to
    // the new location of `this`.
    poll_register::instance()
        .register_callback(raw_fd(), [&](short events) { this->poll(events); });
}

client::client(tcpstream stream, size_t id, std::shared_ptr<message_queue> messages)
    : _stream(std::move(stream))
    , _messages(messages)
    , _id(id)
    , _msg_iter(_messages->cbegin())
{
    register_for_poll(POLLIN);
    register_callback();
}

client::client(client&& rhs)
    : _stream(std::move(rhs._stream))
    , _messages(rhs._messages)
    , _id(rhs._id)
    , _msg_iter(rhs._msg_iter)
{
    // We have just moved, the reference stored in the callback closure is no
    // longer valid, update it.
    register_callback();
}

client& client::operator=(client&& rhs) {
    _recv_buf = std::move(rhs._recv_buf);
    _recv_idx = rhs._recv_idx;
    _send_buf = std::move(rhs._send_buf);
    _connected = rhs._connected;
    _id = rhs._id;
    _stream = std::move(rhs._stream);
    _messages = std::move(rhs._messages);
    _msg_iter = rhs._msg_iter;

    // Move assignment is a move, register the callback once more!
    register_callback();
    return *this;
}

client::~client() {
    if (is_connected()) {
        poll_register::instance().unregister_fd(_stream.fd());
    }
}

void client::poll_recv() {
    ssize_t n_recv = _stream.nonblocking_recv(_recv_buf.data() + _recv_idx, _recv_buf.size() - _recv_idx);
    if (n_recv == 0) {
        disconnect();
        return;
    }

    if (n_recv < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) return;
        THROW_ERRNO("failed to recv");
    }

    if (n_recv > 0) {
        _recv_idx += n_recv;

        // Keep the same size available for the buffer.
        std::fill_n(std::back_inserter(_recv_buf), n_recv, 0);
    }

    auto msg_end = std::find(_recv_buf.begin(), _recv_buf.begin() + _recv_idx, '\n');
    // If we haven't found the end of the message yet, continue.
    if (msg_end == _recv_buf.begin() + _recv_idx) return;

    // End of the message was found! Write the message.
    std::string s;
    std::copy(_recv_buf.begin(), msg_end + 1, std::back_inserter(s));
    push_message(std::move(s));

    // Update the buffer to put the start of the subsequent message in the start
    //
    //                 current message              _recv_idx
    //                 vvvvvvvvvvvvvv                   v
    //     _recv_buf: |message here\npartial message her-----------|
    //                              |^^^^^^^^^^^^^^^^^^^
    //                              |   next message
    //                              ^
    //                            msg_end
    //
    //                                 now can be overwritten
    //                                    vvvvvvvvvvvvvv
    //     _recv_buf: |partial message hermessage here\n-----------|
    //         after   ^^^^^^^^^^^^^^^^^^^|
    //                    next message    |
    //                                    ^
    //                                _recv_idx
    //
    auto new_first = std::rotate(_recv_buf.begin(), msg_end + 1, _recv_buf.begin() + _recv_idx);
    _recv_idx = std::distance(_recv_buf.begin(), new_first);

    return;
}

void client::poll_send() {
    while (1) {
        // If we have nothing to send, check if there is any new message in the channel.
        if (_send_buf.empty()) {
            if (!has_new_messages()) {
                // Nothing else to send, unregister the event.
                unregister_for_poll(POLLOUT);
                return;
            }
            _send_buf = pop_message();
        }
        ssize_t n_sent = _stream.nonblocking_send((const uint8_t*)_send_buf.data(), _send_buf.size());

        if (n_sent < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) return;
            THROW_ERRNO("failed to send");
        }

        if (n_sent == 0) {
            disconnect();
            return;
        }
        if (n_sent > 0) _send_buf = _send_buf.substr(n_sent);

        // There is more stuff to be sent, but we can't do it now since it might
        // block. Continue the work next time we poll.
        if (!_send_buf.empty()) return;

        // If the message was already sent, move on to the next message,
        // since nothing was blocking.
    }
}

void client::poll(short events) {
    // There are new messages inthe queue, we want to start sending them to the client.
    if (has_new_messages()) register_for_poll(POLLOUT);
    if (events & POLLIN ) poll_recv();
    if (events & POLLOUT) poll_send();
}

void client::register_for_poll(short events) {
    poll_register::instance().register_event(raw_fd(), events);
}

void client::unregister_for_poll(short events) {
    poll_register::instance().unregister_event(raw_fd(), events);
}

int client::raw_fd() const { return _stream.fd(); }
bool client::is_connected() const { return _connected; }

void client::disconnect() {
    if (!is_connected()) return;
    std::cout << "client " << _id << " disconnected" << std::endl;
    poll_register::instance().unregister_fd(_stream.fd());
    _connected = false;
    _stream.close();
}

void client::push_message(std::string s) { _messages->push_back(_id, s); }
bool client::has_new_messages() const { return _msg_iter != _messages->cend(); }

// This function may only be called after a call to `has_new_messages` has
// returned true.
std::string_view client::pop_message() {
    return (_msg_iter++)->content;
}
