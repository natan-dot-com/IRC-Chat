#include "db.hpp"

namespace irc {
    channel& db::join_chan(irc::connection *conn, std::string_view channel_name) {
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
        connection_id_t id = conn->id();

        auto chan_it = _channels.find(channel_name);
        channel* ptr = nullptr;
        if (chan_it == _channels.end()) {
            std::cout << "channel " << channel_name << " created with " << id << " as moderator" << std::endl;
            // Here we initialize the channel name as the empty string because we can't yet get a
            // reference to the key where the string will be allocated. This insertion should never
            // fail since we checked there is no other channel with the same key.
            auto [it, _] = _channels.emplace(channel_name, channel("", conn));

            // Now make the name of the channel point to the key.
            it->second._name = it->first;
            channel_name = it->second._name;
            ptr = &it->second;
        } else {
            channel_name = chan_it->second._name;
            chan_it->second.add_member(conn);
            ptr = &chan_it->second;
        }

        auto& info = _connections.at(id);

        // Now `channel_name` points into the string in the key of the `_channels` map.
        info.joined_channel = channel_name;
        return *ptr;
    }

    bool db::quit_chan(connection_id_t id, std::string_view channel_name) {
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

    void db::register_connection(connection_id_t id, uint32_t ipv4) {
        _connections.insert(std::make_pair(id, conn_info(id, ipv4)));
    }

    db::conn_info& db::get_conn_info(connection_id_t id) {
        return _connections.at(id);
    }

    db::conn_info* db::get_conn_info_by_nick(std::string_view nick) {
        auto it = std::find_if(_connections.begin(), _connections.end(),
                               [=](auto& conn) { return conn.second.nick == nick; });
        if (it == _connections.end()) return nullptr;
        return &it->second;
    }

    channel* db::get_channel(std::string_view name) {
        auto it = _channels.find(name);
        if (it == _channels.end()) return nullptr;
        return &it->second;
    }

    void db::remove_connection(connection_id_t id) {
        _connections.erase(id);
    }
}
