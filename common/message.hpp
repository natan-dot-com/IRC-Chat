#ifndef _MESSAGE_QUEUE_H
#define _MESSAGE_QUEUE_H

#include <string>
#include <deque>
#include <functional>
#include <optional>
#include <vector>
#include <variant>
#include <iostream>

#include "utils.hpp"

namespace irc {
    enum numeric_reply {
        RPL_WHOISUSER = 311,
        RPL_CHANNELMODEIS = 324,
        ERR_NOSUCHNICK = 401,
        ERR_NOSUCHCHANNEL = 403,
        ERR_CANNOTSENDTOCHAN = 404,
        ERR_ERRONEUSNICKNAME = 432,
        ERR_NICKNAMEINUSE = 433,
        ERR_NOTONCHANNEL = 442,
        ERR_NEEDMOREPARAMS = 461,
        ERR_ALREADYREGISTERED = 462,
        ERR_CHANOPRIVSNEEDED = 482,
    };

    enum class command {
    //  COMMAND      IRC RFC Section
    //  =======      ===============
        // pass,     // 4.1.1
        nick,        // 4.1.2
        user,        // 4.1.3
        // server,   // 4.1.4
        // oper,     // 4.1.5
        quit,        // 4.1.6
        // squit,    // 4.1.7
        join,        // 4.2.1
        // part,     // 4.2.2
        mode,        // 4.2.3
        // topic,    // 4.2.4
        // names,    // 4.2.5
        // list,     // 4.2.6
        // invite,   // 4.2.7
        kick,        // 4.2.8
        // version,  // 4.3.1
        // status,   // 4.3.2
        // links,    // 4.3.3
        // time,     // 4.3.4
        // connect,  // 4.3.5
        // trace,    // 4.3.6
        // admin,    // 4.3.7
        // info,     // 4.3.8
        privmsg,     // 4.4.1
        // notice,   // 4.4.2
        // who,      // 4.5.1
        whois,       // 4.5.2
        // whowas,   // 4.5.3
        // kill,     // 4.6.1
        // error,    // 4.6.4

        // Diverges from RFC
        ping,        // 4.6.2
        pong,        // 4.6.3
    };



    std::ostream& operator<<(std::ostream& os, enum command cmd);

    struct message {
        class parse_error : public std::exception {
        public:
            parse_error(const char* msg) : _msg(msg) { }
            const char* what() { return _msg; }

        private:
            const char* _msg;
        };

        static message parse(std::string_view s);

        std::optional<std::string> prefix;
        std::variant<enum command, numeric_reply> command;
        std::vector<std::string> params;

        message(std::string prefix, std::variant<enum command, numeric_reply> command,
                std::vector<std::string> params = std::vector<std::string>());

        message(std::variant<enum command, numeric_reply> command,
                std::vector<std::string> params = std::vector<std::string>());
        message() = default;

        std::string to_string() const;


        static inline message no_such_nick() {
            return message("server", ERR_NOSUCHNICK, { "No such nick/channel" });
        }

        static inline message no_such_channel() {
            return message("server", ERR_NOSUCHCHANNEL, { "No such channel" });
        }

        static inline message cannot_send_to_chan() {
            return message("server", ERR_CANNOTSENDTOCHAN, { "Cannot send to channel" });
        }

        static inline message erroneus_nickname() {
            return message("server", ERR_ERRONEUSNICKNAME, { "Erroneus nickname" });
        }

        static inline message nickname_in_use() {
            return message("server", ERR_NICKNAMEINUSE, { "Nickname is already in use" });
        }

        static inline message not_on_channel() {
            return message("server", ERR_NOTONCHANNEL, { "You're not on that channel" });
        }

        static inline message need_more_params(enum command cmd) {
            std::stringstream ss;
            ss << cmd;
            return message("server", ERR_NEEDMOREPARAMS, { ss.str(), "Not enough parameters" });
        }

        static inline message chann_op_priv_needed() {
            return message("server", ERR_CHANOPRIVSNEEDED, { "You're not channel operator" });
        }

        static inline message already_registered() {
            return message("server", ERR_ALREADYREGISTERED, { "You may not reregister" });
        }
    };
}

#endif
