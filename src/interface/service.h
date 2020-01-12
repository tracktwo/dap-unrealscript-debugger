
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
    // EVENTS
    //
    // Events sent from Unreal to the debugger interface. These are all dispatched over the
    // network to the debugger client.
    //
    // With the exception of the 'AddAWatch' entry point, all of these calls return void and
    // communication is entirely from Unreal to the debugger. See AddAWatch for more details.
    /////////////////

    void show_dll_form();
    void build_hierarchy();
    void clear_hierarchy();
    void add_class_to_hierarchy(const char *class_name);
    void clear_a_watch(int watch_kind);
    int add_a_watch(int watch_kind, int parent, const char *name, const char *value);
    void lock_list(int watch_kind);
    void unlock_list(int watch_kind);
    void add_breakpoint(const char* class_name, int line_number);
    void remove_breakpoint(const char* class_name, int line_number);
    void editor_load_class(const char* class_name);
    void editor_goto_line(int line_number, int highlight);
    void add_line_to_log(const char* text);
    void call_stack_clear();
    void call_stack_add(const char* entry);
    void set_current_object_name(const char* object_name);

    /////////////////
    // COMMANDS
    //
    // Commands sent from the debugger client to the debugger interface. These are dispatched
    // to unreal through the callback pointer provided by Unreal as simple strings.
    /////////////////

    void add_breakpoint(const unreal_debugger::commands::AddBreakpoint& cmd);
    void remove_breakpoint(const unreal_debugger::commands::RemoveBreakpoint& cmd);
    void add_watch(const unreal_debugger::commands::AddWatch& cmd);
    void remove_watch(const unreal_debugger::commands::RemoveWatch& cmd);
    void clear_watch(const unreal_debugger::commands::ClearWatch& cmd);
    void change_stack(const unreal_debugger::commands::ChangeStack& cmd);
    void set_data_watch(const unreal_debugger::commands::SetDataWatch& cmd);
    void break_on_none(const unreal_debugger::commands::BreakOnNone& cmd);
    void break_cmd(const unreal_debugger::commands::Break& cmd);
    void stop_debugging(const unreal_debugger::commands::StopDebugging& cmd);
    void go(const unreal_debugger::commands::Go& cmd);
    void step_into(const unreal_debugger::commands::StepInto& cmd);
    void step_over(const unreal_debugger::commands::StepOver& cmd);
    void step_out_of(const unreal_debugger::commands::StepOutOf& cmd);

private:

    using Lock = std::lock_guard<std::mutex>;

    void send_event(const unreal_debugger::events::Event& msg);
    void dispatch_command(const unreal_debugger::commands::Command& cmd);
    void send_next_message();
    void receive_next_message();

    std::thread worker_;

    // Maintain a record of the indices we have assigned to each of the three
   // watch kinds unreal implements. These values are used by clear_a_watch
    // and add_a_watch.
    int watch_indices_[3];

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
