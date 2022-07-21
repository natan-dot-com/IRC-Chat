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
#include "../common/message.hpp"
#include "poll_registry.hpp"
#include "tcplistener.hpp"
#include "connection.hpp"
#include "../common/utils.hpp"
#include "db.hpp"
#include "channel.hpp"

#define PORT 8080

namespace irc {
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

            std::cout << "HANDLING MESSAGE " << std::endl;

            irc::message message;
            try {
                message = irc::message::parse(s);
            } catch (irc::message::parse_error err) {
                std::cerr << "MESSAGE FORMAT ERROR: " << err.what() << std::endl;
                return;
            }

            irc::command cmd = std::get<0>(message.command);

            std::cout << "RECV: " << std::quoted(message.to_string()) << std::endl;

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

                    auto& chan = _db.join_chan(conn, chan_name);
                    chan.send_message(irc::message("system", command::privmsg,
                                                   {chan_name,
                                                    conn_info.nick.value() + " joined"}));
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
}

int main(int argc, char *argv[]) {
    irc::server server(PORT);
    server.run();

    return EXIT_SUCCESS;
}
