
#pragma once

#include <cassert>
#include <deque>

namespace unreal_debugger::serialization
{
    namespace commands
    {
        enum class command_kind : char;
    }

    namespace events
    {
        enum class event_kind : char;
    }

    struct message
    {
        std::unique_ptr<char[]> buf_;
        int len_;
    };

    // A very simple thread-safe wrapper around a deque of messages that exposes
    // a limited interface that the debugger interface and client need.
    //
    // The basic model of both the debugger interface and the debugger client
    // is that "outgoing" messages (commands from the debugger client to the
    // debugger interface, or events from the interface to the debugger client)
    // can occur at any time by the 'producer' thread (Unreal for the interface, the
    // DAP thread for the client) and there may be several messages to send queued
    // up in the outgoing message queue waiting to be sent. These messages are
    // pulled out of the queue and sent over the network one at a time by a single
    // IO service thread.
    //
    // The deque itself is not thread-safe, so all accesses of that object are guarded by
    // a mutex. But other than guarding against concurrent access of the data structure
    // itself the queue is used in limited ways that don't require the lock to be held other
    // than over the primitive actions exposed by this class - the lock itself is
    // not exposed.
    //
    // The message queue is effectively a multiple-producer single-consumer model.
    // Most of the time there is only a single producer thread, but the Unreal docs make
    // no guarantee about what thread(s) may call into the API or how, and control can re-enter
    // the debugger API from the thread that invokes a debugger callback.
    //
    // The producer(s) can only enqueue new messages, cannot remove anything from the queue.
    // The consumer thread can remove elements from the queue, but cannot add anything, and the
    // code currently can only allow a single consumer thread.
    //
    // The operations exposed are 'push' (producer only), 'pop', and 'top'(consumer only). Push
    // and pop operations enqueue and dequeue elements, respectively, but also return a bool
    // indicating whether the queue was empty before the push or after the pop. These return
    // values are used to control registration of handlers to drain the queue: when a push
    // operation returns that the queue was empty before the push, it must register a handler
    // to the IO thread to read and send the message. When a 'pop' operation returns that the queue
    // is empty after popping the just-handled message it must similarly register another handler
    // to handle the next message in the queue. Since the tests for emptiness are performed
    // at the same time as the push/pop and while the lock is held, it is guaranteed that there
    // will always be a handler registered for the front-most element of the queue, but no
    // more than that.
    class locked_message_queue
    {
    public:

        // Peek the top-most message.
        const message& top()
        {
            std::lock_guard<std::mutex> lock(mu_);
            return queue_.front();
        }

        // Pop the front-most message from the queue, and return
        // true if the queue is now empty. If this function returns
        // false then the queue is not empty and the consumer thread
        // is responsible for registering a new handler to process the
        // next element in the queue.
        bool pop()
        {
            std::lock_guard<std::mutex> lock(mu_);
            queue_.pop_front();
            return queue_.empty();
        }

        // Push a new message onto the back of the queue, and return
        // true if the queue was empty before this element was added.
        // If this function returns 'true' the calling producer is
        // responsible for registering a handler to process this element.
        bool push(message&& msg)
        {
            std::lock_guard<std::mutex> lock(mu_);
            bool empty = queue_.empty();
            queue_.push_back(std::move(msg));
            return empty;
        }

    private:
        std::deque<message> queue_;
        std::mutex mu_;
    };

    // Verify that a message has been completely serialized or deserialized:
    // the position of the raw buffer 'buf' should be msg.len_ bytes from the
    // start of msg.buf_.
    inline void verify_message(const message& msg, char* buf)
    {
        assert(msg.len_ == (buf - msg.buf_.get()));
    }

    // Serialization helpers
    inline void serialize_bool(char*& buf, bool b)
    {
        *(bool*)buf = b;
        buf += sizeof(bool);
    }

    inline void serialize_int(char*& buf, int v)
    {
        *(int*)buf = v;
        buf += sizeof(int);
    }

    inline void serialize_string(char*& buf, const std::string& str)
    {
        *(int*)buf = str.size();
        buf += sizeof(int);
        memcpy(buf, str.c_str(), str.size());
        buf += str.size();
    }

    inline void serialize_command_kind(char*& buf, commands::command_kind kind)
    {
        *(commands::command_kind*)buf = kind;
        buf += sizeof(commands::command_kind);
    }

    inline void serialize_event_kind(char*& buf, events::event_kind kind)
    {
        *(events::event_kind*)buf = kind;
        buf += sizeof(events::event_kind);
    }

    // Deserialization helpers
    inline commands::command_kind deserialize_command_kind(char*& buf)
    {
        commands::command_kind k = *(commands::command_kind*)buf;
        buf += sizeof(commands::command_kind);
        return k;
    }

    inline events::event_kind deserialize_event_kind(char*& buf)
    {
        events::event_kind k = *(events::event_kind*)buf;
        buf += sizeof(events::event_kind);
        return k;
    }

    inline int deserialize_int(char*& buf)
    {
        int val = *(int*)buf;
        buf += sizeof(int);
        return val;
    }

    inline bool deserialize_bool(char*& buf)
    {
        bool val = *(bool*)buf;
        buf += sizeof(bool);
        return val;
    }

    inline std::string deserialize_string(char*& buf)
    {
        int len = *(int*)buf;
        buf += sizeof(int);

        std::string str(buf, len);
        buf += len;

        return str;
    }
}