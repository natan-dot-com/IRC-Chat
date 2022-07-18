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
        ERR_CANNOTSENDTOCHAN = 404,
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

        // Diverges from RFC
        ping,
        pong,
    };

    std::ostream& operator<<(std::ostream& os, enum command cmd);

    struct message {
        static message parse(std::string_view s);

        std::optional<std::string> prefix;
        std::variant<enum command, numeric_reply> command;
        std::vector<std::string> params;

        message(std::optional<std::string> prefix, std::variant<enum command, numeric_reply> command,
                std::vector<std::string> params = std::vector<std::string>());

        message(std::variant<enum command, numeric_reply> command, std::vector<std::string> params = std::vector<std::string>());

        std::string to_string() const;
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
