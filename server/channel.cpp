#include "channel.hpp"

namespace irc {
    channel::member& channel::add_member(irc::connection *conn) {
        member member = {
            .conn = conn,
            .is_muted = false,
            .is_operator = false,
        };
        auto[it, _] = members.insert(std::make_pair(conn->id(), member));
        return it->second;
    }

    channel::member* channel::get_member(connection_id_t id) {
        return &members.at(id);
    }

    bool channel::remove_member(connection_id_t id) {
        return members.erase(id) == 1;
    }

    bool channel::mute(connection_id_t id) {
        member* member = get_member(id);
        if (!member) return false;
        member->is_muted = true;
        return true;
    }

    bool channel::unmute(connection_id_t id) {
        member* member = get_member(id);
        if (!member) return false;
        member->is_muted = false;
        return true;
    }

    bool channel::make_operator(connection_id_t id) {
        member* member = get_member(id);
        if (!member) return false;
        member->is_operator = true;
        return true;
    }

    void channel::send_message(irc::message msg) {
        for (auto it = members.begin(); it != members.end(); it++) {
            if (it->second.conn->is_connected()) {
                // Send message and advance the iterator.
                it->second.conn->send_message(msg.to_string());
            }
        }
    }

    // If there are no other operators in the channel, promotes a new user to operator. If a user
    // is promoted, it's id is returned.
    std::optional<connection_id_t> channel::maybe_promote_operator() {
        auto it = std::find_if(members.begin(), members.end(),
                               [](auto& member){ return member.second.is_operator; });
        if (it == members.end() && !members.empty()) {
            auto& member = *members.begin();
            member.second.is_operator = true;
            return member.first;
        }
        return std::nullopt;
    }

    bool channel::empty() const { return members.empty(); }

    std::string_view channel::name() const { return _name; }

    channel::channel(std::string_view name, irc::connection *conn) : _name(name) {
        auto& ref = add_member(conn);
        ref.is_operator = true;
    }
}
