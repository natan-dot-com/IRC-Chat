#include <string>
#include <sstream>
#include <string_view>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <iostream>
#include <thread>
#include <csignal>

#include <ncurses.h>

#include <sys/epoll.h>
#include <sys/eventfd.h>

#include "config.hpp"
#include "tcplistener.hpp"
#include "utils.hpp"
#include "message.hpp"

using namespace std::literals::chrono_literals;

std::mutex print_mutex;
std::atomic<bool> RUN(true);

enum class read_line_result {
    reading,
    ready,
    eof,
};

read_line_result read_line(std::string& buf, WINDOW* w) {
    int ch = wgetch(w);
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
    } else if (ch == 4 /* Ctrl + D sends End-of-Transmission char */) {
        return read_line_result::eof;
    }
    if (ch != '\n') return read_line_result::reading;

    std::scoped_lock<std::mutex> lock(print_mutex);
    wmove(w, 0, 0);
    wclrtoeol(w);
    wrefresh(w);

    return read_line_result::ready;
}

void shutdown(int shutdown_eventfd) {
    RUN = false;
    // Notifiy sleeping threads that we should shutdown
    uint64_t val = 0xff;
    if (::write(shutdown_eventfd, &val, sizeof val) < 0)
        THROW_ERRNO("write to eventfd failed");
}

// Send message loop. This thread will listen for user input and send them to the server when enter
// is pressed. The eventfd will wakeup the thread if it is blocking but should wakeup (probably
// because it should shutdown or stop running).
void send_message(WINDOW* w, tcpstream& cli, int shutdown_eventfd) {
    int epollfd = epoll_create1(0);
    if (epollfd < 0) THROW_ERRNO("epoll_create1 failed");

    struct epoll_event ev;

    ev.events = EPOLLIN;
    ev.data.fd = STDIN_FILENO;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) < 0)
        THROW_ERRNO("epoll_ctl failed");

    ev.events = EPOLLIN;
    ev.data.fd = shutdown_eventfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, shutdown_eventfd, &ev) < 0)
        THROW_ERRNO("epoll_ctl failed");

    std::string recv_buf;
    while (RUN) {
        if (epoll_wait(epollfd, &ev, 1, -1) < 0)
            THROW_ERRNO("epoll_wait failed");

        if (ev.data.fd != STDIN_FILENO) continue;

        switch (read_line(recv_buf, w)) {
            case read_line_result::reading: continue;
            case read_line_result::ready: break;
            case read_line_result::eof:
                shutdown(shutdown_eventfd);
                // Can't just use break since that would only break out of the switch.
                continue;
        }

        std::string input = std::exchange(recv_buf, std::string());
        if (input == "/ping") {
            input = "PING";
        } else if (input == "/quit") {
            input = "QUIT";
            shutdown(shutdown_eventfd);
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

        while (slice.size() > 0) {
            // Get the substring within the maximum allowed size
            std::string s(slice.substr(0, MAX_SIZE));

            std::string_view to_send = s;
            while (to_send.size() > 0) {
                int nsent = cli.send(reinterpret_cast<const uint8_t*>(s.data()), s.size());
                if (nsent < 0) THROW_ERRNO("send failed");
                if (nsent == 0) {
                    shutdown(shutdown_eventfd);
                    break;
                }
                to_send = to_send.substr(nsent);
            }
            if (!RUN) break;

            slice = slice.substr(s.size());
        }
    }
}

// Receive thread. This receives data from the server and displays it in the chat view. The eventfd
// will wakeup the thread if it is suposed to shutdown, but is blocking.
void recv_message(WINDOW* w, tcpstream& cli, int shutdown_eventfd) {
    std::array<uint8_t, MAX_SIZE> recv_buf;
    std::string msg_str;

    int epollfd = epoll_create1(0);
    if (epollfd < 0) THROW_ERRNO("epoll_create1 failed");

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = cli.fd();
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, cli.fd(), &ev) < 0)
        THROW_ERRNO("epoll_ctl failed");

    ev.events = EPOLLIN;
    ev.data.fd = shutdown_eventfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, shutdown_eventfd, &ev) < 0)
        THROW_ERRNO("epoll_ctl failed");

    while (RUN) {
        if (epoll_wait(epollfd, &ev, 1, -1) < 0)
            THROW_ERRNO("epoll_wait failed");

        if (ev.data.fd != cli.fd()) continue;

        int nread = cli.recv(recv_buf.data(), recv_buf.size());
        if (nread < 0) THROW_ERRNO("recv failed");
        if (nread == 0) {
            shutdown(shutdown_eventfd);
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

    static int shutdown_eventfd;

    shutdown_eventfd = eventfd(0, EFD_CLOEXEC);

    // std::signal(SIGINT, [](int){
    //     shutdown(shutdown_eventfd);
    // });

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

    std::thread sender(send_message, input, std::ref(cli), shutdown_eventfd);
    std::thread receiver(recv_message, recv_chat, std::ref(cli), shutdown_eventfd);

    sender.join();
    receiver.join();

    endwin();
    return EXIT_SUCCESS;
}
