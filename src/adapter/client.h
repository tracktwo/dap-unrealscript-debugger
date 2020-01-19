
#include <memory>
#include <deque>
#include <filesystem>
#include <boost/asio/io_context.hpp>
#include "dap/io.h"
#include "commands.pb.h"
#include "events.pb.h"

namespace fs = std::filesystem;

// Logging

extern bool log_enabled;
extern std::shared_ptr<dap::Writer> log_file;

// Options

extern std::vector<fs::path> source_roots;
extern int debug_port;

// Message passing from the debugger adapter to the debugger interface.

struct SerializedCommand
{
    SerializedCommand(std::unique_ptr<char[]> b, size_t l)
        : bytes(std::move(b)), len(l)
    {}

    SerializedCommand() : bytes{}, len{0}
    {}

    std::unique_ptr<char[]> bytes;
    size_t len;
};

extern std::deque<SerializedCommand> send_queue;
void send_command(const unreal_debugger::commands::Command& command);
void dispatch_event(const unreal_debugger::events::Event& ev);

void stop_debugger();

namespace client
{
    namespace commands
    {
        void remove_breakpoint(const std::string& class_name, int line);
        void add_breakpoint(const std::string& class_name, int line);
        void add_watch(const std::string& var_name);
        void remove_watch(const std::string& var_name);
        void clear_watch();
        void change_stack(int stack_id);
        void set_data_watch(const std::string& var_name);
        void break_on_none(bool b);

        void break_cmd();
        void go();
        void step_over();
        void step_into();
        void step_outof();
        void stop_debugging();

        void toggle_watch_info(bool b);
    }
}
