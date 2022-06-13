#include <functional>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <optional>
#include <memory>

#include <csignal>
#include <cstring>

#include <poll.h>

#include "tcpstream.hpp"
#include "message_queue.hpp"
#include "poll_register.hpp"
#include "server.hpp"
#include "connection.hpp"
#include "utils.hpp"

#define PORT 8080
#define SERVER_NAME "localhost"

typedef size_t connection_id_t;

class channel {
    struct member {
        connection_id_t id;
        bool is_muted;
        bool is_operator;
    };

public:
    channel(std::string name, connection_id_t op_id) : name(name) {
        members.push_back((member) {
            .id = op_id,
            .is_muted = false,
            .is_operator = true,
        });
    }

    void add_member(connection_id_t id) {
        members.push_back((member) {
            .id = id,
            .is_muted = false,
            .is_operator = false,
        });
    }

    member* get_member(connection_id_t id) {
        auto it = std::find_if(members.begin(), members.end(),
                               [=](auto& member){ return member.id == id; });
        if (it == members.end()) return nullptr;
        return &*it;
    }

    bool mute(connection_id_t id) {
        member* member = get_member(id);
        if (!member) return false;
        member->is_muted = true;
        return true;
    }

    bool unmute(connection_id_t id) {
        member* member = get_member(id);
        if (!member) return false;
        member->is_muted = false;
        return true;
    }

    bool make_operator(connection_id_t id) {
        member* member = get_member(id);
        if (!member) return false;
        member->is_operator = true;
        return true;
    }

private:
    std::string name;
    std::vector<member> members;
};

class db {
    struct conn_info {
        // As per the RFC, a user could be in multiple channels, but here we
        // only consider a single channel.
        channel* joined_channel;
        uint32_t ipv4;
        std::string nick;
    };

    std::unordered_map<std::string, channel> channels;
    std::vector<conn_info> connections;

public:
    const std::string& get_nick_by_id(connection_id_t id) const {
        return connections.at(id).nick;
    }

    void join_chan(connection_id_t id, std::string channel_name) {
        auto chan = channels.find(channel_name);
        channel* chan_ptr;
        if (chan == channels.end()) {
            // `id` is already added as a member.
            auto [it, ok] = channels.insert({channel_name, channel(channel_name, id)});
            auto& [_, c] = *it;
            chan_ptr = &c;
        } else {
            auto& [_, c] = *chan;
            c.add_member(id);
            chan_ptr = &c;
        }

        auto& info = connections.at(id);

        // This is ok since the standard guarantees that "References and pointers to either key or
        // data stored in the container are only invalidated by erasing that element, even when the
        // corresponding iterator is invalidated." for `std::unordered_map`.
        //
        // Source: https://en.cppreference.com/w/cpp/container/unordered_map
        info.joined_channel = chan_ptr;
    }

    channel* get_current_chan(connection_id_t id) {
        return connections.at(id).joined_channel;
    }

    uint32_t get_ipv4(connection_id_t id) const {
        return connections.at(id).ipv4;
    }
};

static db db;

void poll_server(server& server, std::shared_ptr<irc::message_queue> messages, std::vector<irc::connection>& connections) {
    size_t id = connections.size();

    std::cout << "client " << id << " connected" << std::endl;

    auto stream = server.accept();
    auto& connenction = connections.emplace_back(std::move(stream), id, messages);

    // Poll the client without any specific event. This will allow it to check
    // if there are messages it should send.
    connenction.poll(0);
}

void setup_message_listeners(std::shared_ptr<irc::message_queue> messages) {
    messages->add_listener([=](irc::message_queue::entry& entry) mutable {
            // Here we are diverging from the RFC. In the RFC, PING commands
            // can only be sent by servers and answered by clients. Here we do
            // it the other way around.
            if (entry.message.command == irc::command::ping) {
                std::string nick = db.get_nick_by_id(entry.client_id);
                messages->send_message(irc::message(irc::command::pong, std::vector<std::string>{nick}));
            }

            if (entry.message.command == irc::command::join) {
                db.join_chan(entry.client_id, entry.message.params[0]);
            }

            if (entry.message.command == irc::command::mute) {
                auto chan = db.get_current_chan(entry.client_id);
                chan->mute(entry.client_id);
            }

            if (entry.message.command == irc::command::unmute) {
                auto chan = db.get_current_chan(entry.client_id);
                chan->unmute(entry.client_id);
            }

            if (entry.message.command == irc::command::whois) {
                db.get_ipv4(entry.client_id);
                messages->send_message(irc::message(
                    irc::command::privmsg,
                    std::vector<std::string>{ }
                ));
            }
        });
}

int main() {
    // Interrupt handler that only sets a quit flag when run. This code is not
    // multithreaded and this shouldn't cause many problems.
    static volatile std::sig_atomic_t quit = false;
    std::signal(SIGINT, [](int){ quit = true; });

    server server(PORT);
    server.start();
    std::cout << "Listening localhost, port " << PORT << std::endl;

    auto messages = std::make_shared<irc::message_queue>();
    std::unordered_map<size_t, std::string> client_id_to_nick;
    setup_message_listeners(messages);

    std::vector<irc::connection> connections;

    poll_registry::instance().register_event(server.fd(), POLLIN);
    poll_registry::instance()
        .register_callback(server.fd(),
            [&](short) {
                poll_server(server, messages, connections);
            });

    while (!quit) {
        size_t n_msgs = messages->size();

        // If the poll call failed because of an interrupt, skip this iteration
        // of the loop. Note that if the SIGINT signal was the cause, the `quit`
        // flag will be set ant the loop will exit. If any other error occurs,
        // throw.
        if (poll_registry::instance().poll_and_dispatch() < 0) {
            if (errno == EINTR) continue;
            THROW_ERRNO("poll failed");
        }

        // Use `partition` instead of `reamove_if` because this algorithm will
        // perform less swaps, and we don't care about the order of clients.
        auto it = std::partition(connections.begin(), connections.end(),
                                 [](auto& cli) { return cli.is_connected(); });

        // Delete disconnected clients.
        connections.erase(it, connections.end());

        // New messages have been added, poll every client again without any
        // particular event. This will make them check if there are any new
        // messages they want to start sending.
        if (messages->size() > n_msgs) {
            for (auto& conn : connections) conn.poll(0);
        }
    }

    // All `tcpstream` destructors will run, closing any open connections.

    return 0;
}
