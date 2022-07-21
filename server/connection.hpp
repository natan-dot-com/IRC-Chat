#ifndef _CLIENT_H
#define _CLIENT_H

#include <functional>
#include <optional>
#include <deque>
#include <memory>
#include <vector>

#include "tcpstream.hpp"
#include "../common/message.hpp"
#include "poll_registry.hpp"

namespace irc {

    static const constexpr int buf_size = 4096;

    typedef size_t connection_id_t;

    // The connection class represents a client connected to the server. It is responsible for
    // receiving messages from the associated tcpstream and sending messages through it when they
    // become available in the message queue.
    class connection {
    public:
        using message_handler_type = std::function<void(connection*, std::string)>;

        connection(tcpstream stream, size_t id, message_handler_type on_msg);

        // Can't move the connection. This allows guarantees that once constructed, the `this`
        // pointer is stable an thus can be reference by globals for the lifetime of the object.
        connection(connection&&) = delete;
        connection(const connection&) = delete;
        ~connection();

        // Checks if the client is still connected.
        bool is_connected() const;

        // Get the id of this connection.
        size_t id() const;

        // Enqueues a message to send to the client.
        void send_message(std::string s);
        void send_message(irc::message msg);

        // Disconnects the client from the server. This will close the connection.
        void disconnect();

        uint32_t get_ipv4() const;

    private:
        // Should only be called when data can be received through `_stream`. `poll_recv` will
        // receive data until the operation would block.
        void poll_recv();

        // Should only be called when data can be sent through `_stream`. `poll_send` will send
        // data until the operation would block.
        void poll_send();

        // Accesses the file descriptor of the tcpstream. If the connection is disconnected, -1
        // will be returned.
        int raw_fd() const;

        // Data needed for receiving from the client.
        std::optional<poll_registry::token_type> _recv_tok;

        // Buffer for receiving data. This buffer will always have `buf_size` free of any data. This
        // way the `recv` operation can always receive the same ammount of data at once.
        std::vector<uint8_t> _recv_buf = std::vector<uint8_t>(buf_size, 0);

        // The index of the next byte to receive into `_recv_buf`.
        size_t _recv_idx = 0;

        // Data needed for sending to the client.
        std::optional<poll_registry::token_type> _send_tok;

        // The slice of data that is being sent. This points into `_send_queue`.
        std::string_view _send_buf;

        // The queue of messages to send to through this connection.
        std::deque<std::string> _send_queue;

        bool _connected = true;
        size_t _id;

        tcpstream _stream;

        // A callback that is called whenever a new message is received.
        message_handler_type _on_msg;
    };
}

#endif
