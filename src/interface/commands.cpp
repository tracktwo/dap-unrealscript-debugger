
#include "service.h"

// Handle commands from the debugger. When a command is read from the debugger network
// socket it is deserialized and then dispatched to unreal via the callback function.
//
// These commands are structured, although strictly speaking there is no real reason for
// them to be. Unreal only accepts a string through its callback function, so we could
// have just as easily had the debugger just send the raw strings over the network and
// pass them off to the Unreal callback with no deserialization or re-encoding as strings.
// This is done simply for error checking to try to ensure the commands we get make sense
// instead of trusting a raw string sent over the network.

namespace serialization = unreal_debugger::serialization;
namespace commands = serialization::commands;

// Given the message received over the wire, deserialize it into structured form and
// call the appropriate debugger service function to re-encode it as a string for the
// unreal callback.
void DebuggerService::dispatch_command(const serialization::message& msg)
{
    char* buf = msg.buf_.get();
    commands::command_kind k = serialization::deserialize_command_kind(buf);
    switch (k)
    {
    case commands::command_kind::add_breakpoint: add_breakpoint(commands::add_breakpoint{ msg }); return;
    case commands::command_kind::remove_breakpoint: remove_breakpoint(commands::remove_breakpoint{ msg }); return;
    case commands::command_kind::add_watch: add_watch(commands::add_watch{ msg }); return;
    case commands::command_kind::remove_watch: remove_watch(commands::remove_watch{ msg }); return;
    case commands::command_kind::clear_watch: clear_watch(commands::clear_watch{ msg }); return;
    case commands::command_kind::change_stack: change_stack(commands::change_stack{ msg }); return;
    case commands::command_kind::set_data_watch: set_data_watch(commands::set_data_watch{ msg }); return;
    case commands::command_kind::break_on_none: break_on_none(commands::break_on_none{ msg }); return;
    case commands::command_kind::break_cmd: break_cmd(commands::break_cmd{ msg }); return;
    case commands::command_kind::stop_debugging: stop_debugging(commands::stop_debugging{ msg }); return;
    case commands::command_kind::go: go(commands::go{ msg }); return;
    case commands::command_kind::step_into: step_into(commands::step_into{ msg }); return;
    case commands::command_kind::step_over: step_over(commands::step_over{ msg }); return;
    case commands::command_kind::step_out_of: step_out_of(commands::step_out_of{ msg }); return;
    case commands::command_kind::toggle_watch_info: toggle_watch_info(commands::toggle_watch_info{ msg }); return;
    }

    throw std::runtime_error("Unexpected command type");
}

void DebuggerService::add_breakpoint(const commands::add_breakpoint& cmd)
{
    std::stringstream stream;
    stream << "addbreakpoint " << cmd.class_name_ << " " << cmd.line_number_;
    callback_function(stream.str().c_str());
}

void DebuggerService::remove_breakpoint(const commands::remove_breakpoint& cmd)
{
    std::stringstream stream;
    stream << "removebreakpoint " << cmd.class_name_ << " " << cmd.line_number_;
    callback_function(stream.str().c_str());
}

void DebuggerService::add_watch(const commands::add_watch& cmd)
{
    std::stringstream stream;
    stream << "addwatch " << cmd.var_name_;
    callback_function(stream.str().c_str());
}

void DebuggerService::remove_watch(const commands::remove_watch& cmd)
{
    std::stringstream stream;
    stream << "removewatch " << cmd.var_name_;
    callback_function(stream.str().c_str());
}

void DebuggerService::clear_watch(const commands::clear_watch& cmd)
{
    callback_function("clearwatch");
}

void DebuggerService::change_stack(const commands::change_stack& cmd)
{
    std::stringstream stream;
    stream << "changestack " << cmd.stack_id_;
    callback_function(stream.str().c_str());
}

void DebuggerService::set_data_watch(const commands::set_data_watch& cmd)
{
    std::stringstream stream;
    stream << "setdatawatch " << cmd.var_name_;
    callback_function(stream.str().c_str());
}

void DebuggerService::break_on_none(const commands::break_on_none& cmd)
{
    if (cmd.break_value_)
        callback_function("breakonnone 1");
    else
        callback_function("breakonnone 0");
}

void DebuggerService::break_cmd(const commands::break_cmd& cmd)
{
    callback_function("break");
}

void DebuggerService::stop_debugging(const commands::stop_debugging& cmd)
{
    state = service_state::shutdown;
    callback_function("stopdebugging");
}

void DebuggerService::go(const commands::go& cmd)
{
    callback_function("go");
}

void DebuggerService::step_into(const commands::step_into& cmd)
{
    callback_function("stepinto");
}

void DebuggerService::step_over(const commands::step_over& cmd)
{
    callback_function("stepover");
}

void DebuggerService::step_out_of(const commands::step_out_of& cmd)
{
    callback_function("stepoutof");
}

// toggle_watch_info is not a real unreal command. This pseudo command is used
// by the debugger to tell the interface service that it does not want to receive
// any watch updates. This is typically used to save network traffic when the debugger
// needs to change stacks to retreive line information. Unreal does not send line
// info in the string for a stack frame entry (although the docs claim it does),
// so the only way to get this for other stack frames is to switch frames and wait for
// the EditorGotoLine() call. But switching frames will also send all watch information
// for the new frame, and this is very expensive.
void DebuggerService::toggle_watch_info(const commands::toggle_watch_info& cmd)
{
    send_watch_info_ = cmd.send_watch_info_;

    // The debugger has requested no watch info. Clear out anything that may be
    // pending in the unlock list.
    if (!send_watch_info_)
    {
        pending_unlocks_[0].reset();
        pending_unlocks_[1].reset();
        pending_unlocks_[2].reset();
    }
}


