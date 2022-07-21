#ifndef _CHANNEL_H
#define _CHANNEL_H

#include <unordered_map>
#include <string_view>

#include "connection.hpp"

namespace irc {
    class channel {
    public:
        struct member {
            irc::connection *conn;
            bool is_muted;
            bool is_operator;
        };

        // Returns a member of the channel.
        member* get_member(connection_id_t id);

        // Mutes a connection. Return `false` if unsuccessful.
        bool mute(connection_id_t id);

        // Unmutes a connection. Return `false` if unsuccessful.
        bool unmute(connection_id_t id);

        // Make a connection operator. Returns `false` if unsuccessful.
        bool make_operator(connection_id_t id);

        // Send a message to every member of the channel.
        void send_message(irc::message msg);

        // If there are no other operators in the channel, promotes a new user to operator. If a
        // user is promoted, it's id is returned.
        std::optional<connection_id_t> maybe_promote_operator();

        bool empty() const;

        std::string_view name() const;

    private:
        channel(std::string_view name, irc::connection *conn);

        // Adds a new member to the channel.
        member& add_member(irc::connection *conn);

        // Removes a member from the channel. Return `false` if unsuccessful.
        bool remove_member(connection_id_t id);

        // The name of the channel. Note that this `string_view` **must** point into the key of the map
        // of the `_channels` member in `class db`. This allows the string to be allocated just once.
        std::string_view _name;
        std::unordered_map<connection_id_t, member> members;

        friend class db;
    };
}

#endif
