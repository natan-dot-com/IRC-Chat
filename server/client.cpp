#include <iomanip>
#include <iostream>

#include "tcpstream.hpp"
#include "client.hpp"
#include "utils.hpp"
#include "poll_register.hpp"

client::client(tcpstream stream, size_t id, poll_register& reg, read_msg_fn_t read_msg, write_msg_fn_t write_msg)
    : _stream(std::move(stream))
    , _reg(reg)
    , _read_msg(std::move(read_msg))
    , _write_msg(std::move(write_msg))
{
    _reg.register_event(raw_fd(), POLLIN);
}

client::client(client&& rhs)
    : _stream(std::move(rhs._stream))
    , _reg(rhs._reg)
    , _read_msg(std::move(rhs._read_msg))
    , _write_msg(std::move(rhs._write_msg))
{}

client& client::operator=(client&& rhs) {
    _stream = std::move(rhs._stream);
    _recv_buf = std::move(rhs._recv_buf);
    _recv_idx = rhs._recv_idx;
    _send_buf = std::move(rhs._send_buf);
    return *this;
}

client::~client() { if (raw_fd() > 0) _reg.unregister_fd(_stream.fd()); }

client::poll_result client::poll_recv() {
    ssize_t n_recv = _stream.nonblocking_recv(_recv_buf.data() + _recv_idx, _recv_buf.size() - _recv_idx);
    if (n_recv == 0) return poll_result::closed;
    if (n_recv < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) return poll_result::pending;
        THROW_ERRNO("failed to recv");
    }
    if (n_recv > 0) {
        _recv_idx += n_recv;

        // Keep the same size available for the buffer.
        std::fill_n(std::back_inserter(_recv_buf), n_recv, 0);
    }

    auto msg_end = std::find(_recv_buf.cbegin(), _recv_buf.cbegin() + _recv_idx, '\n');
    if (msg_end == _recv_buf.cbegin() + _recv_idx) return poll_result::pending;

    std::string s;
    std::copy(_recv_buf.cbegin(), msg_end + 1, std::back_inserter(s));
    _write_msg(std::move(s));

    _recv_buf.clear();
    std::fill_n(std::back_inserter(_recv_buf), BUFSIZ, 0);

    return poll_result::pending;
}

client::poll_result client::poll_send() {
    while (1) {
        if (_send_buf.empty()) {
            auto opt_msg = _read_msg();
            if (!opt_msg) {
                // Nothing else to send, unregister for notification.
                _reg.unregister_event(raw_fd(), POLLOUT);
                return poll_result::pending;
            }
            _send_buf = *opt_msg;
        }
        std::cout << "Seding " << std::quoted(_send_buf) << std::endl;
        ssize_t n_sent = _stream.nonblocking_send((const uint8_t*)_send_buf.data(), _send_buf.size());

        if (n_sent < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) return poll_result::pending;
            THROW_ERRNO("failed to send");
        }

        if (n_sent == 0) return poll_result::closed;
        if (n_sent > 0) _send_buf = _send_buf.substr(n_sent);
        if (_send_buf.size() > 0) return poll_result::pending;

        // If the message was already sent, move on to the next message,
        // since nothing was blocking.
    }
}

client::poll_result client::poll(short events) {
    if (events & POLLIN) {
        auto res = poll_recv();
        if (res == poll_result::closed) return res;
    }

    if (events & POLLOUT) {
        auto res = poll_send();
        if (res == poll_result::closed) return res;
    }

    return poll_result::pending;
}

int client::raw_fd() const { return _stream.fd(); }
void client::register_for_poll(short events) { _reg.register_event(raw_fd(), events); }
void client::notify_new_messages() { register_for_poll(POLLOUT); }
