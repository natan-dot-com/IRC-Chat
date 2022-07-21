#ifndef _DB_H
#define _DB_H

#include <optional>
#include <map>

#include "connection.hpp"
#include "channel.hpp"

namespace irc {

    // The `db` class emulates a database that could be used in an actual IRC implementation. As
    // such, it has information about every connection but not the connection objects themselves.
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

        // Put a connection in a channel. If the channel doesn't exist yet, create one and use this
        // connection as the channel moderator.
        channel& join_chan(irc::connection *conn, std::string_view channel_name);

        // Remove a connection from a channel. Returns `false` if the operation is unsuccessful.
        bool quit_chan(connection_id_t id, std::string_view channel_name);

        // Registers a new connection in the database with the given ip.
        void register_connection(connection_id_t id, uint32_t ipv4);

        // Gets the information about a particular conection. It is undefined behavior to call this
        // function when the connectio `id` is not registered in the database.
        conn_info& get_conn_info(connection_id_t id);

        // Gets a connection info by the nick name. Returns `nullptr` if the nickname `nick` is not
        // registered.
        conn_info* get_conn_info_by_nick(std::string_view nick);

        // Gets a channel by name. Returns `nullptr` if there is no registered channel with name
        // `name`.
        channel* get_channel(std::string_view name);

        // Removes a connection from the database. If the connection isn't present, nothing is
        // done.
        void remove_connection(connection_id_t id);

    private:
        std::map<std::string, channel, std::less<>> _channels;
        //                             ^^^^^^^^^^^~~~ needed since we want to compare strings against
        //                                            string views, which are not the same type, but
        //                                            compare the same.

        // Information about each connection. The index is the connection id
        std::unordered_map<connection_id_t, conn_info> _connections;
    };
}

#endif
