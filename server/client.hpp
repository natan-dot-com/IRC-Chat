#ifndef _CLIENT_H
#define _CLIENT_H

#include <functional>
#include <memory>
#include <vector>

#include "tcpstream.hpp"
#include "message_queue.hpp"

// The client class represents a client connected to the server. The client
// class is responsible for receiving messages from the associated tcpstream and
// send messages through it when they become available in the message queue.
class client {
public:

    client(tcpstream stream, size_t id, std::shared_ptr<message_queue> messages);
    client(client&& rhs);
    client(const client&) = delete;
    ~client();

    client& operator=(client&& rhs);

    // Checks if the client is still connected.
    bool is_connected() const;

    // Notify the client that some event has happened. This function is an
    // opportunity for the client to do some nonblocking processing and check what
    // sate has changed since it was last notified. It is also worth noting that
    // thus function may be called sporadically with `events = 0` and it will never
    // block nor consume large amounts of computation time. Calling it with no
    // events presents an opportunity for the client to check on state changes
    // other than the ones registered by the `poll_register`.
    void poll(short events);

private:
    void poll_recv();
    void poll_send();
    void register_for_poll(short events);
    void unregister_for_poll(short events);
    message_queue& messages() const;
    void push_message(std::string s);
    bool has_new_messages() const;
    void register_callback();

    // This function may only be called after a call to `has_new_messages` has
    // returned true.
    std::string_view pop_message();
    void disconnect();
    int raw_fd() const;

    std::vector<uint8_t> _recv_buf = std::vector<uint8_t>(BUFSIZ, 0);
    size_t _recv_idx = 0;
    std::string_view _send_buf;

    bool _connected = true;
    size_t _id;

    tcpstream _stream;
    std::shared_ptr<message_queue> _messages;
    message_queue::const_iterator _msg_iter;
};

#endif
