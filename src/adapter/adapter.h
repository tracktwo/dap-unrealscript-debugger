
#pragma once
#include "commands.h"

namespace unreal_debugger::client
{
    namespace commands = unreal_debugger::serialization::commands;

    void breakpoint_hit();
    void console_message(const std::string& msg);

    void start_adapter();
    void stop_adapter();
}
