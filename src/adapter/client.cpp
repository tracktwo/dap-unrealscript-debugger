#include <exception>

#include "client.h"
#include "debugger.h"
#include "adapter.h"
#include <io.h>
#include <fcntl.h>

static const int default_port = 10077;

boost::asio::io_context ios;
std::unique_ptr<azmq::pair_socket> sock;
std::mutex client_mutex;
Debugger debugger;
bool log_enabled;
std::shared_ptr<dap::Writer> log_file;
std::deque<SerializedCommand> send_queue;

// Schedule an async receive of the next event from the debugger interface.
void receive_next_event()
{
    {
        std::lock_guard<std::mutex> lock{ client_mutex };

        sock->async_receive([](boost::system::error_code& ec, azmq::message& msg, std::size_t len) {

            {
                // Reaquire the lock before dispatching any events.
                std::lock_guard<std::mutex> lock{ client_mutex };

                if (ec)
                {
                    dap::writef(log_file, "receiving event received error: %s", ec.message().c_str());
                }

                unreal_debugger::events::Event ev;
                if (ev.ParseFromArray(msg.data(), msg.size()))
                {
                    dispatch_event(ev);
                }
                else
                {
                    dap::writef(log_file, "failed to parse event.");
                }
            }

            receive_next_event();
        });
    }
}

void dispatch_event(const unreal_debugger::events::Event& ev)
{
    using namespace unreal_debugger::events;

    switch (ev.kind())
    {
    default:
        dap::writef(log_file, "got event %s\n", ev.Kind_Name(ev.kind()).c_str());
    }
}

void send_next_message()
{
    std::lock_guard<std::mutex> lock(client_mutex);

    assert(!send_queue.empty());
    auto&& next_msg = send_queue.front();

    // Begin the async send of the front-most message
    sock->async_send(next_msg.buffer, [len = next_msg.len](boost::system::error_code ec, std::size_t n) {
        bool is_empty = false;

        {
            std::lock_guard<std::mutex> lock(client_mutex);

            // Perform some basic error checking and log the results.
            if (ec)
            {
                dap::writef(log_file, "sending command received error: %s", ec.message().c_str());
            }

            if (n != len)
            {
                dap::writef(log_file, "sending command truncated: wrote %z of %z bytes", n, len);
            }

            // Remove this message from the queue.
            send_queue.pop_front();
            is_empty = send_queue.empty();
        }

        // If the queue was not empty after removing this just-sent message, schedule the async send of the next message in the queue.
        // If the queue was empty the send will be scheduled by the next command that gets enqueued via send_command.
        if (!is_empty)
        {
            send_next_message();
        }
    });
}

// Enqueue the given command to send to the debugger interface.
void send_command(const unreal_debugger::commands::Command& cmd)
{
    bool is_empty = false;

    {
        std::lock_guard<std::mutex> lock(client_mutex);

        dap::writef(log_file, "Sending command %s\n", cmd.Kind_Name(cmd.kind()).c_str());
        std::size_t len = cmd.ByteSizeLong();
        std::unique_ptr<char[]> buf = std::make_unique<char[]>(len);
        cmd.SerializeToArray(buf.get(), len);

        is_empty = send_queue.empty();
        send_queue.emplace_back(std::move(buf), len);
    }

    // If the queue was empty before we added this message, begin the async send.
    if (is_empty)
    {
        send_next_message();
    }
}

void shutdown()
{
   // ios.stop();
}

bool rdy = false;
int main(int argc, char *argv[])
{
    bool debug_mode = false;
    std::thread dap_thread;


    if (argc > 1 && strcmp(argv[1], "debug") == 0)
    {
        debug_mode = true;
    }

    while (!rdy)
    {
        Sleep(1000);
    }

    sock = std::make_unique<azmq::pair_socket>(ios);

    if (debug_mode)
    {
        // In debug mode we are communicating to VS over a tcp port rather than over stdin/stdout.
        // Log directly to stdout.
        log_file = dap::file(stdout);
        log_enabled = true;
    }
    else
    {
        // Change stdin and stdout to binary mode to avoid any translations
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);

        // Setup the log, if necessary
        log_file = dap::file("C:\\users\\jonathan\\output.log");
        log_enabled = true;
    }

    dap::writef(log_file, "Started! %s\n");

    // Connect to the debugger interface
    int port = default_port;
    std::string addr = "tcp://127.0.0.1:" + std::to_string(port);
    sock->connect(addr);


   // create_adapter();
    if (debug_mode)
    {
        start_debug_server();
    }
    else
    {
        dap_thread = std::thread(start_debug_local);
    }

    // Schedule an async read of the next event from the debugger, then let
    // boost::asio do its thing.
    receive_next_event();

    // Start running the main thread loop (responsible for reading events from the
    // debugger interface and dispatching them).
    ios.run();

    term.fire();
  //  term.wait();

    // We return from 'run' when the debugger has asked to shut down. Stop the
    // DAP service.
    stop_adapter();
    if (!debug_mode)
    {
        dap_thread.join();
    }

}
