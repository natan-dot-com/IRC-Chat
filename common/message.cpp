#include <charconv>
#include <sstream>
#include <iterator>
#include <cctype>
#include <charconv>

#include <sys/eventfd.h>
#include <unistd.h>

#include "message.hpp"
#include "utils.hpp"


namespace irc {

    std::ostream& operator<<(std::ostream& os, enum command cmd) {
        switch (cmd) {
            // case irc::command::pass:    os << "PASS";    break;
            case irc::command::nick:    os << "NICK";    break;
            case irc::command::user:    os << "USER";    break;
            case irc::command::privmsg: os << "PRIVMSG"; break;
            // case irc::command::notice:  os << "NOTICE";  break;
            case irc::command::join:    os << "JOIN";    break;
            case irc::command::whois:   os << "WHOIS";   break;
            case irc::command::mode:    os << "MODE";    break;
            case irc::command::ping:    os << "PING";    break;
            case irc::command::pong:    os << "PONG";    break;
            case irc::command::quit:    os << "QUIT";    break;
            case irc::command::kick:    os << "KICK";    break;
        }
        return os;
    }

    /* implementations for `message` */

    message message::parse(std::string_view s) {
        std::optional<std::string> prefix;
        if (s.at(0) == ':') {
            auto space_idx = s.find(" ");
            if (space_idx == std::string_view::npos) throw parse_error("expected space");
            prefix = s.substr(1, space_idx - 1);
            s = s.substr(space_idx + 1);
        }

        std::variant<irc::command, irc::numeric_reply> command;
        auto space_idx = s.find(" ");
        if (space_idx == std::string_view::npos) space_idx = s.size() - 1;
        auto cmd_name = s.substr(0, space_idx);
        s = s.substr(space_idx + 1);

        if      (cmd_name == "USER"    ) command = irc::command::user;
        // else if (cmd_name == "PASS"    ) command = irc::command::pass;
        else if (cmd_name == "NICK"    ) command = irc::command::nick;
        else if (cmd_name == "PRIVMSG" ) command = irc::command::privmsg;
        // else if (cmd_name == "NOTICE"  ) command = irc::command::notice;
        else if (cmd_name == "JOIN"    ) command = irc::command::join;
        else if (cmd_name == "WHOIS"   ) command = irc::command::whois;
        else if (cmd_name == "PING"    ) command = irc::command::ping;
        else if (cmd_name == "PONG"    ) command = irc::command::pong;
        else if (cmd_name == "MODE"    ) command = irc::command::mode;
        else if (cmd_name == "QUIT"    ) command = irc::command::quit;
        else if (cmd_name == "KICK"    ) command = irc::command::kick;
        else if (cmd_name.size() == 3 && std::all_of(cmd_name.cbegin(), cmd_name.cend(), static_cast<int(*)(int)>(&std::isdigit))) {
            int n = 0;
            auto res = std::from_chars(cmd_name.data(), cmd_name.data() + cmd_name.size(), n);
            if (res.ptr == cmd_name.data()) throw parse_error("invalid numeric reply");
            command = (irc::numeric_reply)n;
        }
        else throw parse_error("unsupported command");

        std::vector<std::string> params;
        while (!s.empty() && s.at(0) != '\n') {
            if (s.at(0) == ':') {
                params.emplace_back(s.substr(1, s.size() - 2));
                break;
            }

            auto space_idx = s.find(" ");
            if (space_idx == std::string_view::npos) space_idx = s.size() - 1;
            params.emplace_back(s.substr(0, space_idx));
            s = s.substr(space_idx + 1);
        }

        if (prefix) return message(*prefix, command, params);
        return message(command, params);
    }

    message::message(std::string prefix, std::variant<enum command, numeric_reply> command,
                     std::vector<std::string> params)
        : prefix(prefix)
        , command(command)
        , params(params)
    { }

    message::message(std::variant<enum command, numeric_reply> command, std::vector<std::string> params)
        : command(command)
        , params(params)
    { }

    std::string message::to_string() const {
        std::ostringstream ss;
        if (prefix) ss << ":" << *prefix << " ";
        std::visit([&](auto&& cmd) { ss << cmd << " "; }, command);
        if (params.size() > 0) {
            std::copy(params.cbegin(), params.cend() - 1, std::ostream_iterator<std::string>(ss, " "));
            ss << ":" << params.back();
        }
        ss << std::endl;
        return ss.str();
    }
}
