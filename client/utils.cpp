#include "utils.hpp"

std::mutex printMutex;
std::atomic<bool> RUN(true);

std::string readLine(WINDOW* w) {
    std::string buf;
    int ch;

    do {
        ch = wgetch(w);
        if (ch == KEY_BACKSPACE && !buf.empty()) {
            buf.pop_back();
            wmove(w, 0, 0);
            wclrtoeol(w);
            wprintw(w, "%s", buf.c_str()); 
            wrefresh(w);
        } 
        else if (isprint(ch)) {
            buf.push_back(ch);
            wprintw(w, "%c", ch);
            wrefresh(w);
        }
    } while (RUN && ch != '\n');
    wmove(w, 0, 0);
    wclrtoeol(w);

    return buf;
}

void sendMessage(WINDOW* w, Client& newClient) {
    while (RUN) {
        std::string input = readLine(w);

        int nSplits = input.length() / MAX_SIZE;
        std::vector<std::string> messageList;
        for (auto i = 0; i < nSplits; ++i) {
            std::string subst("[Client " + std::to_string(newClient.id) + "]: ");
            subst.append(input.substr(i * MAX_SIZE, MAX_SIZE));
            subst.push_back('\n');
            messageList.push_back(subst);
        }
        if (input.length() % MAX_SIZE != 0) {
            std::string subst("[Client " + std::to_string(newClient.id) + "]: ");
            subst.append(input.substr(MAX_SIZE * nSplits));
            subst.push_back('\n');
            messageList.push_back(subst);
        }

        for (auto i = messageList.begin(); i != messageList.end(); ++i) {
            newClient.s_send(*i);
        }
    }
}

void recvMessage(WINDOW* w, Client& newClient) {
    while (RUN) {
        std::string rsp;
        int bytesRead = newClient.s_recv(&rsp);
        if (bytesRead != 0) {
            const std::lock_guard<std::mutex> lock(printMutex);
            wprintw(w, "%s", rsp.c_str());
            wrefresh(w);
        }
    }
}
