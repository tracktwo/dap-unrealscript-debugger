
#include "service.h"

namespace serialization = unreal_debugger::serialization;
namespace commands = serialization::commands;

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

void DebuggerService::toggle_watch_info(const commands::toggle_watch_info& cmd)
{
    send_watch_info_ = cmd.send_watch_info_;
    if (!send_watch_info_)
    {
        pending_unlocks_[0].reset();
        pending_unlocks_[1].reset();
        pending_unlocks_[2].reset();
    }
}


