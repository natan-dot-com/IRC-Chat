#include <string>
#include <sstream>
#include <string_view>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <iostream>
#include <thread>

#include <ncurses.h>

#include <sys/epoll.h>

#include "config.hpp"
#include "tcplistener.hpp"
#include "utils.hpp"
#include "message.hpp"

using namespace std::literals::chrono_literals;

std::mutex print_mutex;
std::atomic<bool> RUN(true);

std::string read_line(WINDOW* w) {
    std::string buf;
    int ch;

    do {
        ch = wgetch(w);
        if (ch == KEY_BACKSPACE && !buf.empty()) {
            buf.pop_back();
            std::scoped_lock<std::mutex> lock(print_mutex);
            wmove(w, 0, 0);
            wclrtoeol(w);
            wprintw(w, "%s", buf.c_str());
            wrefresh(w);
        } else if (isprint(ch)) {
            buf.push_back(ch);
            std::scoped_lock<std::mutex> lock(print_mutex);
            wprintw(w, "%c", ch);
            wrefresh(w);
        }
    } while (RUN && ch != '\n');

    std::scoped_lock<std::mutex> lock(print_mutex);
    wmove(w, 0, 0);
    wclrtoeol(w);

    return buf;
}

void send_message(WINDOW* w, tcpstream& cli) {
    while (RUN) {
        std::string input = read_line(w);
        if (input == "/ping") {
            input = "PING";
        } else if (input == "/quit") {
            input = "QUIT";
            RUN = false;
        } else if (input.find("/join") == 0) {
            input = "JOIN " + input.substr(6);
        } else if (input.find("/nickname") == 0) {
            std::string nick = input.substr(10);
            input = "NICK " + nick;
            std::scoped_lock<std::mutex> lock(print_mutex);
            wmove(stdscr, 0, 0);
            wclrtoeol(w);
            wprintw(stdscr, "Connection established - %s\n", nick.c_str());
            wrefresh(stdscr);
        } else if (input.find("/user") == 0) {
            // TODO
            std::stringstream ss;
            ss << "USER ";
            input = input.substr(6);
            size_t next_space = input.find(' ');
            ss << input.substr(0, ++next_space);
            input = input.substr(next_space);

            //   these fields are unused in
            //   the current implementation
            //     vvvvvvvvvvvvvvvvvvvvvvv
            ss << "<hostname> <servername> :" << input;
            input = ss.str();
            wprintw(w, "%s\n", input.c_str());
            wrefresh(w);
        } else if (input.find("/kick") == 0) {
            input = "KICK " + input.substr(6);
        } else if (input.find("/mute") == 0) {
            input = "MODE --- -v " + input.substr(6);
        } else if (input.find("/unmute") == 0) {
            input = "MODE --- +v " + input.substr(8);
        } else if (input.find("/whois") == 0) {
            input = "WHOIS " + input.substr(7);
        } else {
            input = "PRIVMSG --- :" + input;
        }

        input.push_back('\n');
        std::string_view slice = input;

        while (slice.size() > 0 && RUN) {
            // Get the substring within the maximum allowed size
            std::string s(slice.substr(0, MAX_SIZE));

            std::string_view to_send = s;
            while (to_send.size() > 0) {
                int nsent = cli.send(reinterpret_cast<const uint8_t*>(s.data()), s.size());
                if (nsent < 0) THROW_ERRNO("send failed");
                if (nsent == 0) {
                    RUN = false;
                    break;
                }
                to_send = to_send.substr(nsent);
            }

            slice = slice.substr(s.size());
        }
    }
}

void recv_message(WINDOW* w, tcpstream& cli) {
    std::array<uint8_t, MAX_SIZE> recv_buf;
    std::string msg_str;
    while (RUN) {
        int nread = cli.recv(recv_buf.data(), recv_buf.size());
        if (nread < 0) THROW_ERRNO("recv failed");
        if (nread == 0) {
            RUN = false;
            break;
        }

        msg_str.append(recv_buf.cbegin(), recv_buf.cbegin() + nread);
        size_t end_pos = msg_str.find('\n');
        // Not a full message yet
        if (end_pos == std::string::npos) continue;

        irc::message msg;
        try {
            msg = irc::message::parse(std::string_view(msg_str).substr(0, end_pos + 1));
            msg_str = msg_str.substr(end_pos + 1);
        } catch (irc::message::parse_error e) {
            msg_str = msg_str.substr(end_pos + 1);
            std::scoped_lock<std::mutex> lock(print_mutex);
            wprintw(w, "%s\n", e.what());
            wrefresh(w);
            continue;
        }

        if (std::holds_alternative<irc::numeric_reply>(msg.command)) {
            irc::numeric_reply cmd = std::get<irc::numeric_reply>(msg.command);
            if (cmd == irc::RPL_WHOISUSER) {
                wprintw(w, "User has username '%s' and real name '%s' with ip %s\n",
                        msg.params.at(0).c_str(), msg.params.at(3).c_str(), msg.params.at(1).c_str());
            } else {
                // Must be an error
                wprintw(w, "Error %d:", (int)cmd);
                for (auto& param : msg.params) wprintw(w, " %s", param.c_str());
                wprintw(w, "\n");
            }
        } else {
            irc::command cmd = std::get<irc::command>(msg.command);

            switch (cmd) {
                case irc::command::privmsg:
                    wprintw(w, "[%s]: %s\n", msg.prefix.value().c_str(), msg.params.back().c_str());
                    break;
                case irc::command::pong:
                    wprintw(w, "pong\n");
                    break;

                // Other messages are to be ignored by the client (shouldn't even be received).
                default: break;
            }
        }
        wrefresh(w);
    }
}

int main(int argc, char *argv[]) {
    if (argc == 2) {
        std::cout << "Please enter a valid server and port number" << std::endl;
        return EXIT_FAILURE;
    }

    tcpstream cli = tcpstream::connect(argv[1], static_cast<uint16_t>(atoi(argv[2])));

    initscr();
    noecho();
    cbreak();

    mvhline(getmaxy(stdscr) - 2, 0, '-', getmaxx(stdscr));
    mvhline(1, 0, '-', getmaxx(stdscr));

    refresh();
    WINDOW *recv_chat = newwin(getmaxy(stdscr) - 4, getmaxx(stdscr), 2, 0);
    scrollok(recv_chat, true);
    wrefresh(recv_chat);

    refresh();
    WINDOW *input = newwin(1, getmaxx(stdscr), getmaxy(stdscr) - 1, 0);
    scrollok(input, true);
    nodelay(input, true);
    keypad(input, true);
    wrefresh(input);

    refresh();
    wmove(stdscr, 0, 0);
    wprintw(stdscr, "Connection established\n");
    wrefresh(stdscr);

    std::thread sender(send_message, input, std::ref(cli));
    std::thread receiver(recv_message, recv_chat, std::ref(cli));

    sender.join();
    receiver.join();

    endwin();
    return EXIT_SUCCESS;
}
