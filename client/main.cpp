#include "client.hpp"
#include "utils.hpp"

int main(int argc, char *argv[]) {
    if (argc == 1) {
        std::cout << "Please enter a valid port number";
        return -1;
    }

    Client newClient;
    if (newClient.s_connect(atoi(argv[1]))) {
        std::cout << "Connection can't be reached" << std::endl;
        return -1;
    }

    initscr();
    noecho();
    cbreak();

    refresh();
    WINDOW *recvChat = newwin(getmaxy(stdscr)-4, getmaxx(stdscr), 0, 0);
    scrollok(recvChat, true);
    wrefresh(recvChat);

    refresh();
    WINDOW *input = newwin(1, getmaxx(stdscr), getmaxy(stdscr)-1, 0);
    scrollok(input, true);
    nodelay(input, true);
    keypad(input, true);    
    wrefresh(input);

    refresh();
    wprintw(recvChat, "Connection established\n");
    wrefresh(recvChat);

    std::thread sender(sendMessage, input, std::ref(newClient));
    std::thread receiver(recvMessage, recvChat, std::ref(newClient));

    sender.join();
    receiver.join();

    endwin();
    return 0;
}