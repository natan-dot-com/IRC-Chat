#include <functional>
#include <iostream>
#include <vector>
#include <algorithm>
#include <optional>
#include <iomanip>

#include <csignal>
#include <cstring>

#include <poll.h>

#include "tcpstream.hpp"
#include "message_queue.hpp"
#include "poll_register.hpp"
#include "server.hpp"
#include "client.hpp"

#define PORT 8080

int main() {
    static volatile std::sig_atomic_t quit = false;
    std::signal(SIGINT, [](int){ quit = true;});

    server server(PORT);
    server.start();
    std::cout << "Listening localhost, port " << PORT << std::endl;

    poll_register reg;

    reg.register_event(server.fd(), POLLIN);

    message_queue messages;
    std::vector<poll_ev> events;
    std::vector<client> clients;

    for (;;) {
        int ret = reg.poll(events);
        if (ret == -1 && errno == EINTR && quit) break;

        for (const auto& ev : events) {
            if (ev.fd == server.fd()) {

                size_t id = clients.size();

                std::cout << "client " << id << " connected" << std::endl;

                auto write_msg = [&](std::string s) mutable {
                    std::cout << "recvd message " << std::quoted(s) << std::endl;
                    messages.push_back(id, std::move(s));

                    // Just received a message. Notify all other clients.
                    for (auto& client : clients) client.notify_new_messages();
                };

                auto read_msg = [&, it=messages.cbegin()]() mutable -> std::optional<std::string_view> {
                    if (it == messages.cend()) return std::nullopt;
                    return (*it++).content;
                };

                auto stream = server.accept();
                auto& client = clients.emplace_back(std::move(stream), id, reg, std::move(read_msg), std::move(write_msg));
                if (messages.size() > 0) client.notify_new_messages();
            } else {
                // Find the client that was waiting on the notification.
                auto it = std::find_if(clients.begin(), clients.end(),
                                       [&](auto& client){return ev.fd == client.raw_fd();});

                // If no client was found, move on. This really shuldn't happen, but it's not a big deal.
                if (it == clients.end()) continue;
                size_t id = std::distance(clients.begin(), it);
                auto& client = *it;

                switch (client.poll(ev.events)) {
                    case client::poll_result::closed:
                        std::cout << "client " << id << " disconnected" << std::endl;
                        // Remove the client from the list, destructor will do the closing of file descriptors
                        clients.erase(it);
                        break;

                    case client::poll_result::pending: break;
                }
            }
        }
        events.clear();
    }

    // All `tcpstream` destructors will run, closing any open connections.

    return 0;
}
