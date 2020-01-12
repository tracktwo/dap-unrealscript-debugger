
// Initialize the debugger service. Note that Unreal may call this many times,
// so it must track its own state.

#include <memory>
#include <deque>
#include <boost/asio/io_context.hpp>
#include <azmq/socket.hpp>

#include "events.pb.h"
#include "commands.pb.h"

class DebuggerService
{
    public:
    DebuggerService();

    void start();
    void stop();
    void main_loop();

    /////////////////
    // The Unreal API
    /////////////////

    void show_dll_form();
    void build_hierarchy();

    // Commands
    void add_breakpoint(const unreal_debugger::commands::AddBreakpoint& cmd);

    private:

    using Event = unreal_debugger::events::Event;
    using Command = unreal_debugger::commands::Command;
    using Lock = std::lock_guard<std::mutex>;

    void send_event(const Event& msg);
    void dispatch_command(const Command& cmd);
    void send_next_message();
    void receive_next_message();

    std::thread worker_;

    struct SerializedMessage
    {
        SerializedMessage(std::unique_ptr<char[]> b, size_t l)
            : bytes(std::move(b)), len(l), buffer{bytes.get(), len}
        {}
        std::unique_ptr<char[]> bytes;
        size_t len;
        boost::asio::const_buffer buffer;
    };

    // A queue of serialized messages waiting to be sent.
    std::deque<SerializedMessage> send_queue_;

    // Synchronization between the worker thread (reading debugger commands) and
    // the calls from Unreal (writing debugger events).
    std::mutex mu_;

    // The address to which the socket is bound
    std::string addr_;

    // The boost io context for use for all async i/o over the socket
    boost::asio::io_context ios_;

    // The zeromq socket we will communicate over. Debugger/client communication
    // is tightly paired, so use a PAIR socket. We can't support multiple connections
    // to a single debug session.
    azmq::pair_socket socket_;
};

// The callback function back into unreal just takes a simple string argument
// and returns nothing.
using UnrealCallback = void (*)(const char *);
extern UnrealCallback callback_function;

// Exactly one service can exist at a time. This is built when the unreal debugger stats.
extern std::unique_ptr<DebuggerService> service;
