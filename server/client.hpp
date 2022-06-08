#ifndef _CLIENT_H
#define _CLIENT_H

#include <functional>
#include <optional>

#include "tcpstream.hpp"
#include "poll_register.hpp"

class client {
public:
    using read_msg_fn_t = std::function<std::optional<std::string_view>()>;
    using write_msg_fn_t = std::function<void(std::string)>;

    client(tcpstream stream, size_t id, poll_register& reg, read_msg_fn_t read_msg, write_msg_fn_t write_msg);
    client(client&& rhs);
    client(const client&) = delete;
    ~client();

    client& operator=(client&& rhs);

    enum class poll_result {
        pending,
        closed,
    };

    poll_result poll_recv();
    poll_result poll_send();
    poll_result poll(short events);

    int raw_fd() const;
    void register_for_poll(short events);
    void notify_new_messages();

private:
    std::vector<uint8_t> _recv_buf = std::vector<uint8_t>(BUFSIZ, 0);
    size_t _recv_idx = 0;
    std::string_view _send_buf;

    poll_register& _reg;
    read_msg_fn_t _read_msg;
    write_msg_fn_t _write_msg;
    tcpstream _stream;
};

#endif
