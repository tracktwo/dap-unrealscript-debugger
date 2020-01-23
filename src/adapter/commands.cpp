
#include "client.h"

namespace unreal_debugger::client
{

namespace commands = unreal_debugger::serialization::commands;

void remove_breakpoints(const std::string& class_name, int line)
{
    throw std::runtime_error("NYI");
}

void stop_debugging()
{
    send_command(commands::stop_debugging{});
}

void add_breakpoint(const std::string& class_name, int line)
{
    send_command(commands::add_breakpoint{ class_name, line });
}

void break_cmd()
{
    send_command(commands::break_cmd{});
}

void go()
{
    send_command(commands::go{});
}

void step_over()
{
    send_command(commands::step_over{});
}

void step_into()
{
    send_command(commands::step_into{});
}

void step_outof()
{
    send_command(commands::step_out_of{});
}

void change_stack(int stack_id)
{
    send_command(commands::change_stack{ stack_id });
}

void set_data_watch(const std::string& var_name)
{
    send_command(commands::set_data_watch{ var_name });
}

void add_watch(const std::string& var_name)
{
    send_command(commands::add_watch{ var_name });
}

void clear_watch()
{
    send_command(commands::clear_watch{});
}

void toggle_watch_info(bool b)
{
    send_command(commands::toggle_watch_info{ b });
}

}
