#include <string_view>
#include <algorithm>

#include <ncurses.h>

#include "client.hpp"
#include "config.hpp"

std::mutex print_mutex;
std::atomic<bool> RUN(true);

std::string readLine(WINDOW* w) {
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

void send_message(WINDOW* w, client& cli) {
    while (RUN) {
        std::string input = readLine(w);
        std::string_view slice = input;

        wprintw(w, "SEND: '%s'", input.c_str());
        while (slice.size() > 0) {
            wprintw(w, "HERE", input.c_str());
            cli.send(std::string(slice.substr(0, MAX_SIZE)));
            slice = slice.substr(std::min((size_t)MAX_SIZE, slice.size()));
        }
    }
}

void recv_message(WINDOW* w, client& cli) {
    while (RUN) {
        std::string rsp;
        int bytesRead = cli.recv(rsp);
        if (bytesRead != 0) {
            std::scoped_lock<std::mutex> lock(print_mutex);
            wprintw(w, "%s", rsp.c_str());
            wrefresh(w);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        std::cout << "Please enter a valid port number" << std::endl;
        return EXIT_FAILURE;
    }

    client cli;
    if (cli.connect(atoi(argv[1]))) {
        std::cout << "Connection can't be reached" << std::endl;
        return EXIT_FAILURE;
    }

    initscr();
    noecho();
    cbreak();

    refresh();
    WINDOW *recv_chat = newwin(getmaxy(stdscr)-4, getmaxx(stdscr), 0, 0);
    scrollok(recv_chat, true);
    wrefresh(recv_chat);

    refresh();
    WINDOW *input = newwin(1, getmaxx(stdscr), getmaxy(stdscr)-1, 0);
    scrollok(input, true);
    nodelay(input, true);
    keypad(input, true);
    wrefresh(input);

    refresh();
    wprintw(recv_chat, "Connection established\n");
    wrefresh(recv_chat);

    std::thread sender(send_message, input, std::ref(cli));
    std::thread receiver(recv_message, recv_chat, std::ref(cli));

    sender.join();
    receiver.join();

    endwin();
    return EXIT_SUCCESS;
}
