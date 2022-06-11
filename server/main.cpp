#include <functional>
#include <iostream>
#include <vector>
#include <algorithm>
#include <optional>
#include <memory>

#include <csignal>
#include <cstring>

#include <poll.h>

#include "tcpstream.hpp"
#include "message_queue.hpp"
#include "poll_register.hpp"
#include "server.hpp"
#include "client.hpp"
#include "utils.hpp"

#define PORT 8080

void poll_server(server& server, std::shared_ptr<message_queue> messages, std::vector<client>& clients) {
    size_t id = clients.size();

    std::cout << "client " << id << " connected" << std::endl;

    auto stream = server.accept();
    auto& client = clients.emplace_back(std::move(stream), id, messages);

    // Poll the client without any specific event. This will allow it to check
    // if there are messages it should send.
    client.poll(0);
}

int main() {
    // Interrupt handler that only sets a quit flag when run. This code is not
    // multithreaded and this shouldn't cause many problems.
    static volatile std::sig_atomic_t quit = false;
    std::signal(SIGINT, [](int){ quit = true;});

    server server(PORT);
    server.start();
    std::cout << "Listening localhost, port " << PORT << std::endl;

    auto messages = std::make_shared<message_queue>();
    std::vector<client> clients;

    poll_register::instance().register_event(server.fd(), POLLIN);
    poll_register::instance()
        .register_callback(server.fd(),
            [&](short) {
                poll_server(server, messages, clients);
            });

    while (!quit) {
        size_t n_msgs = messages->size();

        // If the poll call failed because of an interrupt, skip this iteration
        // of the loop. Note that if the SIGINT signal was the cause, the `quit`
        // flag will be set ant the loop will exit. If any other error occurs,
        // throw.
        if (poll_register::instance().poll_and_dispatch() < 0) {
            if (errno == EINTR) continue;
            THROW_ERRNO("poll failed");
        }

        // Use `partition` instead of `reamove_if` because this algorithm will
        // perform less swaps, and we don't care about the order of clients.
        auto it = std::partition(clients.begin(), clients.end(),
                                 [](auto& cli) { return cli.is_connected(); });

        // Delete disconnected clients.
        clients.erase(it, clients.end());

        // New messages have been added, poll every client again without any
        // particular event. This will make them check if there are any new
        // messages they want to start sending.
        if (messages->size() > n_msgs) {
            for (auto& client : clients) client.poll(0);
        }
    }

    // All `tcpstream` destructors will run, closing any open connections.

    return 0;
}
