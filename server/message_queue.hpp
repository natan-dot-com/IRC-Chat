#ifndef _MESSAGE_QUEUE_H
#define _MESSAGE_QUEUE_H

#include <string>
#include <deque>
#include <functional>
#include <vector>

// A queue of messages sent by clients. This queue is append only and all
// message references never get invalidated. This means a client may hold a
// reference to any message for as long as it wants without risk of the
// reference beeing invalidated.
class message_queue {
public:
    struct entry {
        size_t sender_id;
        std::string content;
    };

    using queue_type = std::deque<entry>;

    // A queue iterator that is not invalidated by `push_back`. This is possible
    // since it uses indicies instead of pointers in order to track where the
    // iterator is.
    class const_iterator {
    public:
        const_iterator(const const_iterator& rhs);

        bool operator<(const const_iterator& rhs ) const;
        bool operator==(const const_iterator& rhs) const;
        bool operator!=(const const_iterator& rhs) const;

        const_iterator operator=(const const_iterator& rhs);

        const entry& operator*();
        const entry* operator->();
        const_iterator operator++();
        const_iterator operator++(int);

    private:
        const_iterator(const queue_type& queue, size_t idx);

        const queue_type& queue() const;

        std::reference_wrapper<const queue_type> _queue;
        size_t _idx;

        friend class message_queue;
    };

    void push_back(size_t id, std::string s);
    void add_listener(std::function<void(entry&)> callback);
    size_t size() const;
    const_iterator cbegin() const;
    const_iterator cend() const;

private:
    queue_type _messages;
    std::vector<std::function<void(entry&)>> _listeners;

    friend class const_iterator;
};

#endif
