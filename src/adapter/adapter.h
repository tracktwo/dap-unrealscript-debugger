
#pragma once
// Interface into the DAP adapter to communicate with the client UI.

namespace unreal_debugger::adapter
{
    void breakpoint_hit();
    void console_message(const std::string& msg);
    void debugger_terminated();

    void start_adapter();
    void stop_adapter();
}
