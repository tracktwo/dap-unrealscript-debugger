
#include "service.h"

using namespace unreal_debugger::commands;

void DebuggerService::dispatch_command(const Command& cmd)
{
    printf("Got command %s\n", Command::Kind_Name(cmd.kind()).c_str());
    switch (cmd.kind())
    {
    case Command_Kind_AddBreakpoint:
        if (cmd.has_add_breakpoint())
        {
            add_breakpoint(cmd.add_breakpoint());
            return;
        }
        break;

    case Command_Kind_RemoveBreakpoint:
        if (cmd.has_remove_breakpoint())
        {
            remove_breakpoint(cmd.remove_breakpoint());
            return;
        }
        break;

    case Command_Kind_AddWatch:
        if (cmd.has_add_watch())
        {
            add_watch(cmd.add_watch());
            return;
        }
        break;

    case Command_Kind_RemoveWatch:
        if (cmd.has_remove_watch())
        {
            remove_watch(cmd.remove_watch());
            return;
        }
        break;

    case Command_Kind_ClearWatch:
        if (cmd.has_clear_watch())
        {
            clear_watch(cmd.clear_watch());
            return;
        }
        break;

    case Command_Kind_ChangeStack:
        if (cmd.has_change_stack())
        {
            change_stack(cmd.change_stack());
            return;
        }
        break;

    case Command_Kind_SetDataWatch:
        if (cmd.has_set_data_watch())
        {
            set_data_watch(cmd.set_data_watch());
            return;
        }
        break;

    case Command_Kind_BreakOnNone:
        if (cmd.has_break_on_none())
        {
            break_on_none(cmd.break_on_none());
            return;
        }
        break;

    case Command_Kind_Break:
        if (cmd.has_break_cmd())
        {
            break_cmd(cmd.break_cmd());
            return;
        }
        break;

    case Command_Kind_StopDebugging:
        if (cmd.has_stop_debugging())
        {
            stop_debugging(cmd.stop_debugging());
            return;
        }
        break;

    case Command_Kind_Go:
        if (cmd.has_go())
        {
            go(cmd.go());
            return;
        }
        break;

    case Command_Kind_StepInto:
        if (cmd.has_step_into())
        {
            step_into(cmd.step_into());
            return;
        }
        break;

    case Command_Kind_StepOver:
        if (cmd.has_step_over())
        {
            step_over(cmd.step_over());
            return;
        }
        break;

    case Command_Kind_StepOutOf:
        if (cmd.has_step_out_of())
        {
            step_out_of(cmd.step_out_of());
            return;
        }
        break;
    }

    // If we get here then either we have an invalid command: either the payload didn't match
    // the kind or it had an unknown kind. Send an error back over to the client.
    std::stringstream stream;
    stream << "Internal Debugger Error: bad command or payload " << cmd.kind();
    add_line_to_log(stream.str().c_str());
}

void DebuggerService::add_breakpoint(const AddBreakpoint& cmd)
{
    std::stringstream stream;
    stream << "addbreakpoint " << cmd.class_name() << " " << cmd.line_number();
    callback_function(stream.str().c_str());
}

void DebuggerService::remove_breakpoint(const RemoveBreakpoint& cmd)
{
    std::stringstream stream;
    stream << "removebreakpoint " << cmd.class_name() << " " << cmd.line_number();
    callback_function(stream.str().c_str());
}

void DebuggerService::add_watch(const AddWatch& cmd)
{
    std::stringstream stream;
    stream << "addwatch " << cmd.var_name();
    callback_function(stream.str().c_str());
}

void DebuggerService::remove_watch(const RemoveWatch& cmd)
{
    std::stringstream stream;
    stream << "removewatch " << cmd.var_name();
    callback_function(stream.str().c_str());
}

void DebuggerService::clear_watch(const unreal_debugger::commands::ClearWatch& cmd)
{
    callback_function("clearwatch");
}

void DebuggerService::change_stack(const unreal_debugger::commands::ChangeStack& cmd)
{
    std::stringstream stream;
    stream << "changestack " << cmd.stack_id();
    callback_function(stream.str().c_str());
}

void DebuggerService::set_data_watch(const unreal_debugger::commands::SetDataWatch& cmd)
{
    std::stringstream stream;
    stream << "setdatawatch " << cmd.var_name();
    callback_function(stream.str().c_str());
}

void DebuggerService::break_on_none(const unreal_debugger::commands::BreakOnNone& cmd)
{
    if (cmd.break_value())
        callback_function("breakonnone 1");
    else
        callback_function("breakonnone 0");
}

void DebuggerService::break_cmd(const unreal_debugger::commands::Break& cmd)
{
    callback_function("break");
}

void DebuggerService::stop_debugging(const unreal_debugger::commands::StopDebugging& cmd)
{
    state = service_state::shutdown;
    // TODO Does this work?
    callback_function("stopdebugging");
}

void DebuggerService::go(const unreal_debugger::commands::Go& cmd)
{
    callback_function("go");
}

void DebuggerService::step_into(const unreal_debugger::commands::StepInto& cmd)
{
    callback_function("stepinto");
}

void DebuggerService::step_over(const unreal_debugger::commands::StepOver& cmd)
{
    callback_function("stepover");
}

void DebuggerService::step_out_of(const unreal_debugger::commands::StepOutOf& cmd)
{
    callback_function("stepoutof");
}

void DebuggerService::toggle_watch_info(const unreal_debugger::commands::ToggleWatchInfo& cmd)
{
    send_watch_info_ = cmd.send_watch_info();
}

