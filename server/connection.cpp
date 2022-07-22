#include <iostream>
#include <iomanip>
#include <algorithm>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "tcpstream.hpp"
#include "connection.hpp"
#include "utils.hpp"
#include "poll_registry.hpp"

#define MSG_SIZE_LIMIT 4096

using namespace irc;

connection::connection(tcpstream stream, size_t id, message_handler_type on_msg)
    : _stream(std::move(stream))
    , _id(id)
    , _on_msg(on_msg)
{
    _recv_tok = poll_registry::instance()
        .register_event(raw_fd(), POLLIN,
                        [&](short) { this->poll_recv(); });
}

connection::~connection() {
    // If we are already unregistered, that's fine, it will just do nothing.
    if (is_connected()) disconnect();
}

void connection::poll_recv() {
    ssize_t n_recv = _stream.nonblocking_recv(_recv_buf.data() + _recv_idx,
                                              _recv_buf.size() - _recv_idx);
    if (n_recv == 0) {
        disconnect();
        return;
    }

    if (n_recv < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) return;
        THROW_ERRNO("failed to recv");
    }

    _recv_idx += n_recv;

    // Keep the same size available for the buffer.
    _recv_buf.resize(_recv_idx + buf_size, 0);

    auto msg_end = std::find(_recv_buf.begin() + _recv_idx - n_recv,
                             _recv_buf.begin() + _recv_idx, '\n');

    // If we haven't found the end of the message yet, continue.
    if (msg_end == _recv_buf.begin() + _recv_idx) return;

    // End of the message was found! Write the message.
    std::string s;
    std::copy(_recv_buf.begin(), msg_end + 1, std::back_inserter(s));
    _on_msg(this, std::move(s));

    // Update the buffer to put the start of the subsequent message in the start of the buffer.
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
    _recv_buf.resize(_recv_idx + buf_size, 0);

    return;
}

void connection::poll_send() {
    while (1) {
        // If we have nothing to send, check if there is any new message in the channel.
        if (_send_buf.empty()) {
            if (_send_queue.empty()) {
                // Nothing else to send, unregister the event.
                poll_registry::instance().unregister_event(*_send_tok);
                _send_tok = std::nullopt;
                return;
            }
            // `_send_buf` will hold a reference to the front of the queue.
            _send_buf = _send_queue.front();
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

        _send_buf = _send_buf.substr(n_sent);

        // There is more stuff to be sent, but we can't do it now since it might block. Continue
        // the work next time we poll.
        if (!_send_buf.empty()) return;

        // We have just finished sending a message that was beeing referenced by `_send_buf`. Now
        // we can pop it.
        _send_queue.pop_front();

        // If the message was already sent, move on to the next message, since nothing was
        // blocking.
    }
}

void connection::send_message(std::string s) {
    // Split the message into smaller chunks
    std::string_view slice = s;
    do {
        _send_queue.emplace_back(slice.substr(0, MSG_SIZE_LIMIT));
        slice = slice.substr(std::min((size_t)MSG_SIZE_LIMIT, slice.size()));
    } while (slice.size() > 0);
    if (!_send_tok) {
        _send_tok = poll_registry::instance()
            .register_event(raw_fd(), POLLOUT, [&](short){ this->poll_send(); });
    }
}

void connection::send_message(irc::message msg) { send_message(msg.to_string()); }

int connection::raw_fd() const { return _stream.fd(); }
bool connection::is_connected() const { return _connected; }
size_t connection::id() const { return _id; }

void connection::disconnect() {
    if (!is_connected()) return;
    std::cout << "client " << _id << " disconnected" << std::endl;
    if (_recv_tok) poll_registry::instance().unregister_event(*_recv_tok);
    if (_send_tok) poll_registry::instance().unregister_event(*_send_tok);
    _connected = false;
    _stream.close();
}

uint32_t connection::get_ipv4() const {
    struct sockaddr_in address;
    size_t addrlen = sizeof(address);
    int ret = getpeername(_stream.fd(), (struct sockaddr*)&address, (socklen_t*)&addrlen);
    if (ret < 0) THROW_ERRNO("getpeername failed");
    // TODO: Check if this is in host or network byte order.
    return htonl(address.sin_addr.s_addr);
}
