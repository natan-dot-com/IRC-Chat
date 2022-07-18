#include <functional>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <map>
#include <unordered_set>
#include <algorithm>
#include <optional>
#include <memory>
#include <iterator>

#include <csignal>
#include <cstring>
#include <cstdlib>

#include <poll.h>

#include "tcpstream.hpp"
#include "message_queue.hpp"
#include "poll_registry.hpp"
#include "tcplistener.hpp"
#include "connection.hpp"
#include "utils.hpp"

#define PORT 8080
#define SERVER_NAME "localhost"

typedef size_t connection_id_t;

class channel {
    struct member {
        connection_id_t id;
        // This allows easy access to the connection. If the connection is destroyed, it can be
        // detected by the `channel` class and the member can be safely removed.
        std::weak_ptr<irc::connection> conn;
        bool is_muted;
        bool is_operator;
    };

public:
    void add_member(std::weak_ptr<irc::connection> conn) {
        auto ptr = conn.lock();
        if (!ptr) return; // Connection no longer valid.
        auto& ref = members.emplace_back((member) {
            .id = ptr->id(),
            .conn = ptr,
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

    bool remove_member(connection_id_t id) {
        auto it = std::find_if(members.begin(), members.end(),
                               [=](auto& member){ return member.id == id; });
        if (it == members.end()) return false;
        members.erase(it);
        return true;
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

    void send_message(irc::message msg) {
        // Past the end iterator for valid members of the channel.
        auto last = members.end();
        for (auto it = members.begin(); it != last;) {
            // Try to promote the `weak_ptr`.
            auto ptr = it->conn.lock();
            if (ptr && ptr->is_connected()) {
                // Send message and advance the iterator.
                ptr->send_message(msg.to_string());
                it++;
            } else {
                // If the connection is no loger valid (has been destroyed) or if it is
                // disconnected by it is still in the channel, swap it with the last element and
                // decrement the past the end iterator `last`. This will mark the element at `it`
                // to be erased by the end of the loop. Here we don't increment `it` since we have
                // just swapped it for the last element and have not yet processed it.
                std::iter_swap(it, --last);
            }
        }

        members.erase(last, members.end());
    }

private:
    channel(std::string_view name, connection_id_t id, std::weak_ptr<irc::connection> conn) : name(name) {
        members.emplace_back((member) {
            .id = id,
            .conn = conn,
            .is_muted = false,
            .is_operator = true,
        });
    }

    // The name of the channel. Note that this `string_view` **must** point into the key of the map
    // of the `_channels` member in `class db`. This allows the string to be allocated just once.
    std::string_view name;
    std::vector<member> members;

    friend class db;
};

class db {
    struct conn_info {
        // As per the RFC, a user could be in multiple channels, but here we only consider a single
        // channel.
        //
        // This `string_view` points into a key of the `_channels` member.
        std::string_view joined_channel;
        uint32_t ipv4;
        std::string nick;

        conn_info() = default;
    };

    std::map<std::string, channel, std::less<>> _channels;
    //                             ^^^^^^^^^^^~~~ needed since we want to compare strings against
    //                                            string views, which are not the same type, but
    //                                            compare the same.

    // Information about each connection. The index is the connection id
    std::vector<conn_info> _connections;

public:
    void join_chan(std::weak_ptr<irc::connection> conn, std::string_view channel_name) {
        // When creating a channel, we use the string at the key of the map `_channels` as the only
        // allocation for the channel name. All other instances of the name of the channel are
        // `string_view`s that point into this key. This is ok since the standard of C++17
        // says for AssociativeContainer (`std::map` is an AssociativeContainer):
        //
        //     Section 26.2.6:
        //     ===============
        //
        //     The insert and emplace members shall not affect the validity of iterators and
        //     references to the container, and the erase members shall invalidate only
        //     iterators and references to the erased elements.
        //
        // so as long as the key is not removed from the map, it can be safely referenced.

        auto ptr = conn.lock();
        if (!ptr) return; // connection is no longer valid.
        connection_id_t id = ptr->id();

        auto chan_it = _channels.find(channel_name);
        if (chan_it == _channels.end()) {
            auto nick = get_nick_by_id(id);
            std::cout << "channel " << channel_name << " created with " << nick << " as moderator" << std::endl;
            // Here we initialize the channel name as the empty string because we can't yet get a
            // reference to the key where the string will be allocated. This insertion should never
            // fail since we checked there is no other channel with the same key.
            auto [it, _] = _channels.emplace(channel_name, channel("", id, conn));

            // Now make the name of the channel point to the key.
            it->second.name = it->first;
            channel_name = it->second.name;
        } else {
            channel_name = chan_it->second.name;
            chan_it->second.add_member(conn);
        }

        auto& info = _connections.at(id);

        // Now `channel_name` points into the string in the key of the `_channels` map.
        info.joined_channel = channel_name;

    }

    bool quit_chan(connection_id_t id, std::string_view channel_name) {
        auto chan = get_channel(channel_name);
        if (!chan || !chan->remove_member(id)) return false;

        // Chennal is empty, remove it
        if (chan->members.size() == 0) {
            std::cout << "channel " << channel_name << " deleted since it had no members" << std::endl;
            _channels.erase(_channels.find(channel_name));
        }
        return true;
    }

    void register_nick(connection_id_t id, std::string nick) {
        auto it = std::find_if(_connections.begin(), _connections.end(),
                               [&](auto& info) { return info.nick == nick; });
        if (it == _connections.end()) {
            if (id >= _connections.size()) _connections.resize(id + 1);
            _connections.at(id).nick = std::move(nick);
        } else {
            it->nick = nick;
        }
    }

    const std::string& get_nick_by_id(connection_id_t id) const {
        return _connections.at(id).nick;
    }

    const std::optional<connection_id_t> get_id_by_nick(std::string_view nick) const {
        auto it = std::find_if(_connections.cbegin(), _connections.cend(),
                               [=](auto& info){ return info.nick == nick; });

        if (it == _connections.cend()) return std::nullopt;
        return std::distance(_connections.cbegin(), it);
    }

    // The returned `string_view` is only valid for the lifetime of the channel. If a longer
    // lifetime is required, turn it into a `string`.
    std::string_view get_current_chan(connection_id_t id) {
        return _connections.at(id).joined_channel;
    }

    channel* get_channel(std::string_view name) {
        auto it = _channels.find(name);
        if (it == _channels.end()) return nullptr;
        return &it->second;
    }

    uint32_t get_ipv4(connection_id_t id) const {
        return _connections.at(id).ipv4;
    }
};

class server {
public:
    server(uint16_t port) : _listener(port) { }
    server(const server&) = delete;
    server(server&&) = delete;
    ~server() {
        poll_registry::instance().unregister_event(_listener_tok);
    }

    void run() {
        _listener.start();
        _listener_tok = poll_registry::instance()
            .register_event(_listener.fd(), POLLIN, [&](short) { this->poll_accept(); });

        std::cout << "Listening localhost, port " << PORT << std::endl;

        // Interrupt handler that only sets a quit flag when run. This code is not
        // multithreaded and this shouldn't cause many problems.
        static volatile std::sig_atomic_t quit = false;
        std::signal(SIGINT, [](int){ quit = true; });


        while (!quit) {
            // If the poll call failed because of an interrupt, skip this iteration
            // of the loop. Note that if the SIGINT signal was the cause, the `quit`
            // flag will be set ant the loop will exit. If any other error occurs,
            // throw.
            if (poll_registry::instance().poll_and_dispatch() < 0) {
                if (errno == EINTR) continue;
                THROW_ERRNO("poll failed");
            }

            auto it = std::remove_if(_connections.begin(), _connections.end(),
                                     [](auto& cli) { return !cli->is_connected(); });

            // All connections that are about to close, quit all of their channels.
            for (auto i = it; i < _connections.end(); i++) {
                auto chan_name = _db.get_current_chan((*i)->id());
                _db.quit_chan((*i)->id(), chan_name);
            }

            // Delete disconnected clients.
            _connections.erase(it, _connections.end());
        }

        // All `tcpstream` destructors will run, closing any open connections.
    }

    void poll_accept() {
        // TODO: This cannot be because then if a user exists and reenters, he could end up with the same
        // id as another one.
        connection_id_t id = _connections.size();

        std::cout << "client " << id << " connected" << std::endl;

        auto stream = _listener.accept();

        // All this is necessary because we want to create the `shared_ptr` **before** we construct
        // the `irc::connection`. This is because we will need a `weak_ptr` of the shared inside
        // the closure that is passed to the constructor of `irc::connection`.
        //
        // This is not ideal, because the `shared_ptr` will have to allocate a separate place for
        // the reference count. But it should work.
        std::shared_ptr<irc::connection> ptr((irc::connection*)std::malloc(sizeof(irc::connection)));
        new (ptr.get()) irc::connection(std::move(stream), id,
                                        [this, ptr=std::weak_ptr(ptr)](std::string s) {
                                            // This closure only owns a `weak_ptr`, so it won't
                                            // cause a reference cycle.
                                            this->handle_message(ptr.lock(), std::move(s));
                                        });
        _connections.emplace_back(std::move(ptr));
    }

    void handle_message(std::shared_ptr<irc::connection> conn, std::string s) {
        // Should never happen!
        if (!conn) std::terminate();
        auto id = conn->id();

        irc::message message = irc::message::parse(s);
        irc::command cmd = std::get<0>(message.command);


        switch (cmd) {
            case irc::command::nick:
            {
                auto& nick = message.params.at(0);
                std::cout << "client " << id << " registered as " << nick << std::endl;
                _db.register_nick(id, nick);
                return;
            }

            case irc::command::ping:
            {
                // Here we are diverging from the RFC. In the RFC, PING commands
                // can only be sent by servers and answered by clients. Here we do
                // it the other way around.
                std::string nick = _db.get_nick_by_id(id);
                conn->send_message(irc::message(irc::command::pong));
                return;
            }

            case irc::command::join:
            {
                std::cout << "client " << id << " joins channel " << message.params.at(0) << std::endl;
                _db.join_chan(conn, message.params.at(0));
                return;
            }

            case irc::command::mode:
            {
                const auto& chan_name = message.params.at(0);
                auto chan = _db.get_channel(chan_name);

                if (!chan) {
                    conn->send_message(irc::message(irc::ERR_NOSUCHNICK, { "No such nick/channel" }));
                    return;
                }

                const auto& modifiers = message.params.at(1);
                const auto& nick = message.params.at(2);
                auto target_id = _db.get_id_by_nick(nick);
                if (!target_id) UNIMPLEMENTED();

                if      (modifiers.find("+v") != std::string::npos) chan->mute(*target_id);
                else if (modifiers.find("-v") != std::string::npos) chan->unmute(*target_id);

                // TODO: implement more modifiers.
                return;
            }

            case irc::command::whois:
            {
                const auto& nick = message.params.at(0);
                auto target_id = _db.get_id_by_nick(nick);
                if (!target_id) UNIMPLEMENTED();
                uint32_t ipv4 = _db.get_ipv4(*target_id);

                std::ostringstream ss;
                ss << ((ipv4 >> 24) & 0xff) << "."
                   << ((ipv4 >> 16) & 0xff) << "."
                   << ((ipv4 >>  8) & 0xff) << "."
                   << (ipv4 & 0xff);

                conn->send_message(irc::message(irc::RPL_WHOISUSER, { "<user>", ss.str(), "*", "<real name>" }));
                return;
            }

            case irc::command::privmsg:
            {

                std::string_view channel_name = message.params.at(0);
                auto chan = _db.get_channel(channel_name);
                if (!chan) {
                    conn->send_message(irc::message(irc::ERR_NOSUCHNICK, { "No such nick/channel" }));
                    return;
                }

                auto member = chan->get_member(id);
                if (!member || member->is_muted) {
                    conn->send_message(irc::message(irc::ERR_CANNOTSENDTOCHAN, { std::string(channel_name), "Cannot send to channel" }));
                    return;
                }

                std::cout << "client " << id << " sent message " << std::quoted(message.params.back())
                          << " on channel " << channel_name << std::endl;

                auto nick = _db.get_nick_by_id(id);
                message.prefix = nick;
                chan->send_message(message);
                return;
            }

            case irc::command::quit:
            {
                std::cout << "client " << id << " quitting now" << std::endl;
                // Just mark it as disconnected and close the connection. The actual connection
                // object will be destroyed in the `run` loop sometime soon.
                conn->disconnect();
                return;
            }

            default: UNIMPLEMENTED();
        }
    }

private:
    db _db;
    std::vector<std::shared_ptr<irc::connection>> _connections;
    tcplistener _listener;
    poll_registry::token_type _listener_tok;
};

int main() {
    server server(PORT);
    server.run();

    return 0;
}
