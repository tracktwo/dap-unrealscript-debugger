
#pragma once

#include <memory>
#include <deque>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <atomic>
#include <optional>

#include "events.h"
#include "commands.h"

namespace unreal_debugger::interface
{

namespace serialization = unreal_debugger::serialization;
namespace commands = unreal_debugger::serialization::commands;
namespace events = unreal_debugger::serialization::events;

//////////////////////////////////////////////////////////////////////////////
// Service control
//////////////////////////////////////////////////////////////////////////////

// Start the debugger service
void start_debugger_service();

// Test if the debugger service is running, starting if it not and not
// in shutdown mode.
bool check_service();

enum class service_state : char
{
    // The service is not currently running, or has encountered an error. When in this state any attempt to interact with
    // the debugger service will attempt to shut down any existing service, then start a new one.
    stopped,

    // The service is currently running, but we do not have an active connection
    disconnected,

    // The service is running and is connected to a debug client.
    connected,

    // We have received a shutdown request from the client. The service should be stopped, and not restarted.
    shutdown
};

extern std::atomic<service_state> state;

// An object representing the debugger state.
class debugger_service
{
public:
    debugger_service();
    ~debugger_service() = default;

    void start();
    void stop();
    void shutdown();

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

    void dispatch_command(const serialization::message& msg);
    void add_breakpoint(const commands::add_breakpoint& cmd);
    void remove_breakpoint(const commands::remove_breakpoint& cmd);
    void add_watch(const commands::add_watch& cmd);
    void remove_watch(const commands::remove_watch& cmd);
    void clear_watch(const commands::clear_watch& cmd);
    void change_stack(const commands::change_stack& cmd);
    void set_data_watch(const commands::set_data_watch& cmd);
    void break_on_none(const commands::break_on_none& cmd);
    void break_cmd(const commands::break_cmd& cmd);
    void stop_debugging(const commands::stop_debugging& cmd);
    void go(const commands::go& cmd);
    void step_into(const commands::step_into& cmd);
    void step_over(const commands::step_over& cmd);
    void step_out_of(const commands::step_out_of& cmd);
    void toggle_watch_info(const commands::toggle_watch_info& cmd);

private:
    void send_event(const events::event& ev);
    void send_next_message();
    void receive_next_message();
    void accept_connection();
    void fatal_error(const char* msg, ...);

    // Maintain a record of the indices we have assigned to each of the three
   // watch kinds unreal implements. These values are used by clear_a_watch
    // and add_a_watch.
    int watch_indices_[3];

    // In order to optimize sending watch info we buffer all "AddAWatch" API calls
    // into a single message that will be sent when the watch list is unlocked.
    // This relies on the fact that Unreal consistently locks and unlocks the list
    // around any AddAWatch call.
    std::optional<events::unlock_list> pending_unlocks_[3];

    // If true, we are sending watch info to the client. If false, all lock, unlock,
    // and add watch events are silently discarded.
    bool send_watch_info_ = true;

    // A queue of serialized messages waiting to be sent.
    serialization::locked_message_queue send_queue_;
    
    // The command message currently being read from the debugger client.
    // There is only a single element, not a queue, because only a single
    // thread reads these messages and they are processed entirely before the
    // next one is read.
    serialization::message next_command_;

    using tcp = boost::asio::ip::tcp;

    // Listening acceptor and the connected socket.
    std::unique_ptr<tcp::acceptor> acceptor_;
    std::unique_ptr<tcp::socket> socket_;
};

// The callback function back into unreal just takes a simple string argument
// and returns nothing.
using UnrealCallback = void (*)(const char *);
extern UnrealCallback callback_function;

// Exactly one service can exist at a time. This is built when the unreal debugger stats.
extern std::unique_ptr<debugger_service> service;

}

