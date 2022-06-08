#include <string>
#include <deque>
#include <functional>

class message_queue {
public:
    struct entry {
        size_t sender_id;
        std::string content;
    };

    using queue_type = std::deque<entry>;

    class const_iterator {
    public:
        const_iterator(const const_iterator& rhs)
            : _queue(rhs._queue)
            , _idx(rhs._idx)
        { }

        bool operator<(const const_iterator& rhs)  { return _idx <  rhs._idx; }
        bool operator==(const const_iterator& rhs) { return _idx == rhs._idx; }
        bool operator!=(const const_iterator& rhs) { return _idx != rhs._idx; }

        const_iterator operator=(const const_iterator&& rhs) {
            _idx = rhs._idx;
            _queue = rhs._queue;
            return *this;
        }

        const entry& operator*() { return queue()[_idx]; }
        const entry* operator->() { return &queue()[_idx]; }

        const_iterator operator++() {
            ++_idx;
            return *this;
        }

        const_iterator operator++(int) {
            auto copy = *this;
            ++_idx;
            return copy;
        }

    private:
        const_iterator(const queue_type& queue, size_t idx) : _queue(queue), _idx(idx) {}

        const queue_type& queue() { return _queue.get(); }

        friend class message_queue;

        std::reference_wrapper<const queue_type> _queue;
        size_t _idx;
    };

    void push_back(size_t id, std::string s) {
        _messages.push_back((entry) {
            .sender_id = id,
            .content = std::move(s),
        });
    }
    size_t size() const { return _messages.size(); }
    const_iterator cbegin() const { return const_iterator(_messages, 0); }
    const_iterator cend() const { return const_iterator(_messages, _messages.size()); }

private:
    queue_type _messages;

    friend class const_iterator;
};
