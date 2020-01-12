
#include <memory>
#include <deque>
#include <boost/asio/io_context.hpp>
#include <azmq/socket.hpp>
#include "dap/io.h"
#include "commands.pb.h"
#include "events.pb.h"

// Logging

extern bool log_enabled;
extern std::shared_ptr<dap::Writer> log_file;

// Message passing from the debugger adapter to the debugger interface.

struct SerializedCommand
{
    SerializedCommand(std::unique_ptr<char[]> b, size_t l)
        : bytes(std::move(b)), len(l), buffer{boost::asio::buffer(bytes.get(), len)}
    {}

    std::unique_ptr<char[]> bytes;
    size_t len;
    boost::asio::const_buffer buffer;
};

extern std::deque<SerializedCommand> send_queue;
void send_command(const unreal_debugger::commands::Command& command);
void dispatch_event(const unreal_debugger::events::Event& ev);

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
    }
}
