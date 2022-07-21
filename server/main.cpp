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
#include "message.hpp"
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
        irc::connection *conn;
        bool is_muted;
        bool is_operator;
    };

public:
    void add_member(irc::connection *conn) {
        auto& ref = members.emplace_back((member) {
            .id = conn->id(),
            .conn = conn,
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
        for (auto it = members.begin(); it != members.end(); it++) {
            if (it->conn->is_connected()) {
                // Send message and advance the iterator.
                it->conn->send_message(msg.to_string());
                it++;
            }
        }
    }

    // If there are no other operators in the channel, promotes a new user to operator. If a user
    // is promoted, it's id is returned.
    std::optional<connection_id_t> maybe_promote_operator() {
        auto it = std::find_if(members.begin(), members.end(),
                               [](auto& member){ return member.is_operator; });
        if (it == members.end() && !members.empty()) {
            auto& member = *members.begin();
            member.is_operator = true;
            return member.id;
        }
        return std::nullopt;
    }

    bool empty() const { return members.empty(); }

    std::string_view name() const { return _name; }

private:
    channel(std::string_view name, connection_id_t id, irc::connection *conn) : _name(name) {
        members.emplace_back((member) {
            .id = id,
            .conn = conn,
            .is_muted = false,
            .is_operator = true,
        });
    }

    // The name of the channel. Note that this `string_view` **must** point into the key of the map
    // of the `_channels` member in `class db`. This allows the string to be allocated just once.
    std::string_view _name;
    std::vector<member> members;

    friend class db;
};

class db {
public:
    enum class conn_state {
        init,
        registered_nick,
        registered_user,
    };

    struct conn_info {
        // As per the RFC, a user could be in multiple channels, but here we only consider a single
        // channel.
        //
        // This `string_view` points into a key of the `_channels` member.
        std::optional<std::string_view> joined_channel = std::nullopt;
        std::optional<std::string> nick = std::nullopt;
        std::optional<std::string> realname = std::nullopt;
        std::optional<std::string> username = std::nullopt;
        conn_state state = conn_state::init;
        uint32_t ipv4;
        connection_id_t id;

        conn_info(connection_id_t id, uint32_t ipv4) : id(id), ipv4(ipv4) { }
    };

    void join_chan(irc::connection *conn, std::string_view channel_name) {
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

        if (!conn) return; // connection is no longer valid.
        connection_id_t id = conn->id();

        auto chan_it = _channels.find(channel_name);
        if (chan_it == _channels.end()) {
            std::cout << "channel " << channel_name << " created with " << id << " as moderator" << std::endl;
            // Here we initialize the channel name as the empty string because we can't yet get a
            // reference to the key where the string will be allocated. This insertion should never
            // fail since we checked there is no other channel with the same key.
            auto [it, _] = _channels.emplace(channel_name, channel("", id, conn));

            // Now make the name of the channel point to the key.
            it->second._name = it->first;
            channel_name = it->second._name;
        } else {
            channel_name = chan_it->second._name;
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
        if (chan->empty()) {
            std::cout << "channel " << channel_name << " deleted since it had no members" << std::endl;
            _channels.erase(_channels.find(channel_name));
        } else {
            auto promoted = chan->maybe_promote_operator();
            if (promoted) {
                auto promoted_info = get_conn_info(*promoted);
                std::cout << "promoting " << *promoted_info.nick << std::endl;
                chan->send_message(irc::message(
                    "system",
                    irc::command::privmsg,
                    { std::string(channel_name), *promoted_info.nick + " promoted to operator" }
                ));
            }
        }
        return true;
    }

    void register_connection(connection_id_t id, uint32_t ipv4) {
        _connections.insert(std::make_pair(id, conn_info(id, ipv4)));
    }

    conn_info& get_conn_info(connection_id_t id) {
        return _connections.at(id);
    }

    conn_info* get_conn_info_by_nick(std::string_view nick) {
        auto it = std::find_if(_connections.begin(), _connections.end(),
                               [=](auto& conn) { return conn.second.nick == nick; });
        if (it == _connections.end()) return nullptr;
        return &it->second;
    }

    channel* get_channel(std::string_view name) {
        auto it = _channels.find(name);
        if (it == _channels.end()) return nullptr;
        return &it->second;
    }

    void remove_connection(connection_id_t id) {
        _connections.erase(id);
    }

private:
    std::map<std::string, channel, std::less<>> _channels;
    //                             ^^^^^^^^^^^~~~ needed since we want to compare strings against
    //                                            string views, which are not the same type, but
    //                                            compare the same.

    // Information about each connection. The index is the connection id
    std::unordered_map<connection_id_t, conn_info> _connections;
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

            std::vector<connection_id_t> to_remove;

            // All connections that are about to close, quit all of their channels.
            for (auto i = _connections.begin(); i != _connections.end();) {
                connection_id_t id = i->second->id();
                if (!i->second->is_connected()) {
                    auto info = _db.get_conn_info(id);
                    if (info.joined_channel) {
                        auto chan = _db.get_channel(*info.joined_channel);
                        chan->send_message(irc::message(info.nick.value(), irc::command::privmsg,
                                                        {std::string(*info.joined_channel),
                                                         info.nick.value() + " quit"}));
                        _db.quit_chan(id, *info.joined_channel);
                    }
                    _db.remove_connection(id);

                    // We can't erase the element at the iterator's position and then increment the
                    // interator. Therefore we must increment it **before** we erase the element.
                    i++;
                    _connections.erase(id);
                } else {
                    i++;
                }
            }
        }

        // All `tcpstream` destructors will run, closing any open connections.
    }

    void poll_accept() {
        // TODO: This cannot be because then if a user exists and reenters, he could end up with the same
        // id as another one.
        connection_id_t id = _curr_id_count++;

        std::cout << "client " << id << " connected" << std::endl;

        auto stream = _listener.accept();

        auto ptr = std::make_unique<irc::connection>(std::move(stream), id,
                                                     [this](auto ptr, std::string s) {
                                                         this->handle_message(ptr, std::move(s));
                                                     });
        const auto&[it, ok] = _connections.emplace(std::make_pair(id, std::move(ptr)));
        _db.register_connection(id, it->second->get_ipv4());
    }

    void handle_message(irc::connection *conn, std::string s) {
        // Should never happen!
        if (!conn) std::terminate();
        auto id = conn->id();
        auto& conn_info = _db.get_conn_info(id);

        irc::message message;
        try {
            message = irc::message::parse(s);
        } catch (irc::message::parse_error err) {
            std::cerr << "MESSAGE FORMAT ERROR: " << err.what() << std::endl;
            return;
        }

        irc::command cmd = std::get<0>(message.command);

        // First command must be a NICK.
        if (conn_info.state == db::conn_state::init && cmd != irc::command::nick) return;

        // After a NICK command, must send a USER command.
        if (conn_info.state == db::conn_state::registered_nick && cmd != irc::command::user) return;

        switch (cmd) {
            // Ignore
            case irc::command::pong: return;

            case irc::command::nick:
            {
                if (message.params.size() < 1) {
                    conn->send_message(irc::message::need_more_params(cmd));
                    return;
                }

                auto& nick = message.params.at(0);
                if (nick.size() > 50) {
                    conn->send_message(irc::message::erroneus_nickname());
                    return;
                }

                if (_db.get_conn_info_by_nick(nick)) {
                    conn->send_message(irc::message::nickname_in_use());
                    return;
                }

                std::cout << "client " << id << " registered as " << nick << std::endl;
                conn_info.nick = nick;
                if (conn_info.state == db::conn_state::init) {
                    conn_info.state = db::conn_state::registered_nick;
                }
                return;
            }

            case irc::command::user:
            {
                // Command: USER
                // Parameters: <username> <hostname> <servername> <realname>
                //
                // For the purposes of this implementation, <hostname> and <servername> are
                // ignored.

                if (message.params.size() < 4) {
                    conn->send_message(irc::message::need_more_params(cmd));
                    return;
                }

                if (conn_info.state == db::conn_state::registered_user) {
                    conn->send_message(irc::message::already_registered());
                    return;
                }

                conn_info.username = message.params.at(0);
                conn_info.realname = message.params.at(3);
                conn_info.state = db::conn_state::registered_user;
                return;
            }

            case irc::command::ping:
            {
                // Here we are diverging from the RFC. In the RFC, PING commands
                // can only be sent by servers and answered by clients. Here we do
                // it the other way around.
                conn->send_message(irc::message(irc::command::pong));
                return;
            }

            case irc::command::join:
            {
                if (message.params.size() < 1) {
                    conn->send_message(irc::message::need_more_params(cmd));
                    return;
                }

                auto& chan_name = message.params.at(0);
                if (chan_name.size() == 0
                 || chan_name.size() > 200
                 || (chan_name[0] != '#' && chan_name[0] != '&')
                 || chan_name.find(',') != std::string::npos) {
                    conn->send_message(irc::message::no_such_channel());
                    return;
                }

                if (conn_info.joined_channel) _db.quit_chan(id, *conn_info.joined_channel);

                _db.join_chan(conn, chan_name);
                return;
            }

            case irc::command::mode:
            {
                if (message.params.size() < 3) {
                    conn->send_message(irc::message::need_more_params(cmd));
                    return;
                }

                const auto& chan_name = message.params.at(0);
                auto chan = _db.get_channel(chan_name);

                if (!chan) {
                    conn->send_message(irc::message::no_such_channel());
                    return;
                }

                auto member = chan->get_member(id);
                if (!member) {
                    conn->send_message(irc::message::not_on_channel());
                    return;
                }

                if (!member->is_operator) {
                    conn->send_message(irc::message::chann_op_priv_needed());
                    return;
                }

                const auto& modifiers = message.params.at(1);
                auto target_id = _db.get_conn_info_by_nick(message.params.at(2));
                if (!target_id) {
                    conn->send_message(irc::message::no_such_nick());
                    return;
                }

                bool ok = true;
                if      (modifiers.find("+v") != std::string::npos) ok = chan->mute(target_id->id);
                else if (modifiers.find("-v") != std::string::npos) ok = chan->unmute(target_id->id);

                if (!ok) {
                    // TODO: Should probably be a better message. This happens when trying to alter
                    // the permissions of a user that exists but is not on the channel. It's not
                    // that the operator isn't on the channel.
                    conn->send_message(irc::message::not_on_channel());
                    return;
                }

                // TODO: implement more modifiers.
                return;
            }

            case irc::command::whois:
            {
                auto target = _db.get_conn_info_by_nick(message.params.at(0));
                if (!target) {
                    conn->send_message(irc::message::no_such_nick());
                    return;
                }

                uint32_t ipv4 = target->ipv4;

                std::ostringstream ss;
                ss << ((ipv4 >> 24) & 0xff) << "."
                   << ((ipv4 >> 16) & 0xff) << "."
                   << ((ipv4 >>  8) & 0xff) << "."
                   << (ipv4 & 0xff);

                conn->send_message(irc::message(irc::RPL_WHOISUSER,
                                                {target->username.value_or("uknown"),
                                                 ss.str(), "*",
                                                 target->realname.value_or("uknown")}));
                return;
            }

            case irc::command::privmsg:
            {
                if (message.params.size() < 2) {
                    conn->send_message(irc::message::need_more_params(cmd));
                    return;
                }

                std::string_view channel_name = message.params.at(0);
                auto chan = _db.get_channel(channel_name);
                if (!chan) {
                    conn->send_message(irc::message::no_such_channel());
                    return;
                }

                auto member = chan->get_member(id);
                if (!member) {
                    conn->send_message(irc::message::not_on_channel());
                    return;
                }

                if (member->is_muted) {
                    conn->send_message(irc::message::cannot_send_to_chan());
                    return;
                }

                std::cout << "client " << id << " sent message " << std::quoted(message.params.back())
                          << " on channel " << channel_name << std::endl;

                auto nick = conn_info.nick.value();
                message.prefix = nick;
                chan->send_message(message);
                return;
            }

            case irc::command::quit:
            {
                std::string quit_msg = *conn_info.nick + " quit";
                if (message.params.size() >= 1) {
                    quit_msg = message.params.at(1);
                }

                std::cout << "client " << id << " quitting now" << std::endl;
                if (conn_info.joined_channel) {
                    auto chan = _db.get_channel(*conn_info.joined_channel);
                    chan->send_message(irc::message(*conn_info.nick, irc::command::privmsg, { quit_msg }));
                    _db.quit_chan(id, *conn_info.joined_channel);
                    conn_info.joined_channel = std::nullopt;
                }
                // Just mark it as disconnected and close the connection. The actual connection
                // object will be destroyed in the `run` loop sometime soon.
                conn->disconnect();
                return;
            }

            case irc::command::kick:
            {
                if (message.params.size() < 2) {
                    conn->send_message(irc::message::need_more_params(cmd));
                    return;
                }

                std::string& chan_name = message.params.at(0);
                auto chan = _db.get_channel(chan_name);
                if (!chan) {
                    conn->send_message(irc::message::not_on_channel());
                    return;
                }

                auto member = chan->get_member(id);
                if (!member) {
                    conn->send_message(irc::message::not_on_channel());
                    return;
                }

                if (!member->is_operator) {
                    conn->send_message(irc::message::chann_op_priv_needed());
                    return;
                }

                auto& kicked_nick = message.params.at(1);
                auto kicked = _db.get_conn_info_by_nick(kicked_nick);
                if (!kicked) {
                    conn->send_message(irc::message::no_such_nick());
                    return;
                }

                bool ok = _db.quit_chan(kicked->id, chan_name);
                if (!ok) {
                    conn->send_message(irc::message::not_on_channel());
                    return;
                }

                std::cout << "client " << kicked_nick << " was kicked" << std::endl;
                return;
            }
        }
    }

private:
    db _db;
    connection_id_t _curr_id_count = 0;
    std::map<connection_id_t, std::unique_ptr<irc::connection>> _connections;
    tcplistener _listener;
    poll_registry::token_type _listener_tok;
};

int main(int argc, char *argv[]) {
    server server(PORT);
    server.run();

    return 0;
}
