#include <exception>

#include "client.h"
#include "debugger.h"
#include "adapter.h"
#include "signals.h"

#include <io.h>
#include <fcntl.h>
#include <boost/asio/ip/tcp.hpp>

static const int default_port = 10077;

namespace asio = boost::asio;
using tcp = boost::asio::ip::tcp;

boost::asio::io_context ios;
std::unique_ptr<tcp::socket> sock;
SerializedCommand next_event;

namespace signals {
    signal stack_changed;
}

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

        sock->async_receive(boost::asio::buffer(&next_event.len, 4), [](const boost::system::error_code& ec, std::size_t len) {

            {
                // Reaquire the lock before dispatching any events.
                std::lock_guard<std::mutex> lock{ client_mutex };

                if (ec)
                {
                    dap::writef(log_file, "receiving event header error: %s", ec.message().c_str());
                    debugger_terminated();
                    return;
                }

                if (len != 4)
                {
                    dap::writef(log_file, "failed to read next message header: read %z of 4 bytes\n", len);
                    debugger_terminated();
                    return;
                }

                // TODO Reuse existing buffer if it's big enough.
                next_event.bytes = std::make_unique<char[]>(next_event.len);

                sock->async_receive(boost::asio::buffer(next_event.bytes.get(), next_event.len), [](const boost::system::error_code& ec, std::size_t len) {
                    if (ec)
                    {
                        dap::writef(log_file, "receiving event body error: %s", ec.message().c_str());
                        debugger_terminated();
                        return;
                    }

                    if (len != next_event.len)
                    {
                        dap::writef(log_file, "failed to read next message body: received %z of %z bytes\n", len, next_event.len);
                        debugger_terminated();
                        return;
                    }
                    unreal_debugger::events::Event ev;
                    if (ev.ParseFromArray(next_event.bytes.get(), len))
                    {
                        dispatch_event(ev);
                    }
                    else
                    {
                        dap::writef(log_file, "failed to parse event.");
                    }

                    receive_next_event();
                });
            }
        });
    }
}

void send_next_message()
{
    std::lock_guard<std::mutex> lock(client_mutex);

    assert(!send_queue.empty());
    auto&& next_msg = send_queue.front();

    // Begin the async send of the front-most message
    sock->async_send(asio::buffer(&next_msg.len, 4), [](const boost::system::error_code& ec, std::size_t n) {
        if (ec)
        {
            dap::writef(log_file, "sending command header received error: %s", ec.message().c_str());
        }

        if (n != 4)
        {
            dap::writef(log_file, "sending command header truncated: wrote %z of 4 bytes", n);
        }

        // Now send the message body
        auto&& next_msg = send_queue.front();
        sock->async_send(asio::buffer(next_msg.bytes.get(), next_msg.len), [len = next_msg.len](const boost::system::error_code& ec, std::size_t n) {
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

// Begin the shutdown process: This stops the IO process, which will allow the main
// thread to begin the cleanup of the DAP connection and ultimately exit the process.
void stop_debugger()
{
   ios.stop();
}

int main(int argc, char *argv[])
{
    bool debug_mode = false;

    if (argc > 1 && strcmp(argv[1], "debug") == 0)
    {
        debug_mode = true;
    }

    // TODO error handling for failed connection

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

    dap::writef(log_file, "Started!\n");

    sock = std::make_unique<tcp::socket>(ios);
    boost::system::error_code ec;
    sock->connect(tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), default_port), ec);
    if (ec)
    {
        dap::writef(log_file, "Connection to debugger failed: %s\n", ec.message().c_str());
        // TODO Error message.
        return 1;
    }

   // create_adapter();
    if (debug_mode)
    {
        start_debug_server();
    }
    else
    {
        start_debug_local();
    }

    // Schedule an async read of the next event from the debugger, then let
    // boost::asio do its thing.
    receive_next_event();

    // Start running the main thread loop (responsible for reading events from the
    // debugger interface and dispatching them).
    ios.run();

    // We return from 'run' when the debugger has asked to shut down. Shut down the
    // dap service.

    if (debug_mode)
    {
        stop_debug_server();
    }
    log_file->close();
    stop_adapter();
}

