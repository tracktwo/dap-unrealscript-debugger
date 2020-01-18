
#include "client.h"
#include "commands.pb.h"

namespace client
{
    namespace commands
    {
        using namespace unreal_debugger::commands;

        void remove_breakpoints(const std::string& class_name, int line)
        {
            throw std::runtime_error("NYI");
        }

        void stop_debugging()
        {
            Command cmd;
            cmd.set_kind(Command_Kind_StopDebugging);
            cmd.mutable_stop_debugging();
            send_command(cmd);
        }

        void add_breakpoint(const std::string& class_name, int line)
        {
            AddBreakpoint add_msg;
            Command cmd;

            cmd.set_kind(Command_Kind_AddBreakpoint);
            add_msg.set_class_name(class_name);
            add_msg.set_line_number(line);
            std::swap(*cmd.mutable_add_breakpoint(), add_msg);

            send_command(cmd);
        }

        void break_cmd()
        {
            Break break_msg;
            Command cmd;

            cmd.set_kind(Command_Kind_Break);
            cmd.mutable_break_cmd();
            send_command(cmd);
        }

        void go()
        {
            Go go_msg;
            Command cmd;

            cmd.set_kind(Command_Kind_Go);
            cmd.mutable_go();
            send_command(cmd);
        }

        void step_over()
        {
            StepOver step_msg;
            Command cmd;

            cmd.set_kind(Command_Kind_StepOver);
            cmd.mutable_step_over();
            send_command(cmd);
        }

        void step_into()
        {
            StepInto step_msg;
            Command cmd;

            cmd.set_kind(Command_Kind_StepInto);
            cmd.mutable_step_into();
            send_command(cmd);
        }

        void step_outof()
        {
            StepOutOf step_msg;
            Command cmd;

            cmd.set_kind(Command_Kind_StepOutOf);
            cmd.mutable_step_out_of();
            send_command(cmd);
        }

        void change_stack(int stack_id)
        {
            ChangeStack change_stack;
            Command cmd;

            cmd.set_kind(Command_Kind_ChangeStack);
            cmd.mutable_change_stack()->set_stack_id(stack_id);
            send_command(cmd);
        }

        void set_data_watch(const std::string& var_name)
        {
            SetDataWatch watch;
            Command cmd;

            cmd.set_kind(Command_Kind_SetDataWatch);
            cmd.mutable_set_data_watch()->set_var_name(var_name);
            send_command(cmd);
        }

        void add_watch(const std::string& var_name)
        {
            AddWatch watch;
            Command cmd;

            cmd.set_kind(Command_Kind_AddWatch);
            cmd.mutable_add_watch()->set_var_name(var_name);
            send_command(cmd);
        }

        void toggle_watch_info(bool b)
        {
            ToggleWatchInfo toggle_watch_info;
            Command cmd;

            cmd.set_kind(Command_Kind_ToggleWatchInfo);
            cmd.mutable_toggle_watch_info()->set_send_watch_info(b);
            send_command(cmd);
        }
    }
}
