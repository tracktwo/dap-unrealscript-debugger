
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

    }
}
