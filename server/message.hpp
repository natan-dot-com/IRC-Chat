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
        ERR_CHANOPRIVSNEEDED = 482,
    };

    enum class command {
        pass,
        nick,
        user,
        privmsg,
        notice,
        join,
        whois,
        mode,
        quit,
        kick,

        // Diverges from RFC
        ping,
        pong,
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

        message(std::optional<std::string> prefix, std::variant<enum command, numeric_reply> command,
                std::vector<std::string> params = std::vector<std::string>());

        message(std::variant<enum command, numeric_reply> command, std::vector<std::string> params = std::vector<std::string>());
        message() = default;

        std::string to_string() const;


        static inline message no_such_nick() {
            return message(ERR_NOSUCHNICK, { "No such nick/channel" });
        }

        static inline message no_such_channel() {
            return message(ERR_NOSUCHCHANNEL, { "No such channel" });
        }

        static inline message cannot_send_to_chan() {
            return message(ERR_CANNOTSENDTOCHAN, { "Cannot send to channel" });
        }

        static inline message erroneus_nickname() {
            return message(ERR_ERRONEUSNICKNAME, { "Erroneus nickname" });
        }

        static inline message nickname_in_use() {
            return message(ERR_NICKNAMEINUSE, { "Nickname is already in use" });
        }

        static inline message not_on_channel() {
            return message(ERR_NOTONCHANNEL, { "You're not on that channel" });
        }

        static inline message need_more_params(enum command cmd) {
            std::stringstream ss;
            ss << cmd;
            return message(ERR_NEEDMOREPARAMS, { ss.str(), "Not enough parameters" });
        }

        static inline message chann_op_priv_needed() {
            return message(ERR_CHANOPRIVSNEEDED, { "You're not channel operator" });
        }
    };

    /*
    // A queue of messages sent by clients. This queue is append only and all
    // message references never get invalidated. This means a client may hold a
    // reference to any message for as long as it wants without risk of the
    // reference beeing invalidated.
    class message_queue {
    public:

        struct entry {
            size_t client_id;
            message message;
        };

        using queue_type = std::deque<entry>;

        // A queue iterator that is not invalidated by `push_back`. This is possible
        // since it uses indicies instead of pointers in order to track where the
        // iterator is.
        class const_iterator {
        public:
            const_iterator(const const_iterator& rhs);

            bool operator<(const const_iterator& rhs ) const;
            bool operator==(const const_iterator& rhs) const;
            bool operator!=(const const_iterator& rhs) const;

            const_iterator operator=(const const_iterator& rhs);

            const entry& operator*();
            const entry* operator->();
            const_iterator operator++();
            const_iterator operator++(int);

        private:
            const_iterator(const queue_type& queue, size_t idx);

            const queue_type& queue() const;

            std::reference_wrapper<const queue_type> _queue;
            size_t _idx;

            friend class message_queue;
        };

        message_queue();

        void send_message(message message);
        size_t size() const;
        const_iterator cbegin() const;
        const_iterator cend() const;

    private:
        queue_type _messages;

        friend class const_iterator;
    };
    */
}

#endif
