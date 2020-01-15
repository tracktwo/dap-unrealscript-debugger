

void start_debug_server();
void stop_debug_server();
void start_debug_local();
void stop_adapter();

// Debugger events to send to the debug client.
void breakpoint_hit();
void console_message(const std::string& msg);
void debugger_terminated();


