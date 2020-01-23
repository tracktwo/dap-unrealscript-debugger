#include <exception>

#include "client.h"
#include "debugger.h"
#include "adapter.h"
#include "signals.h"

#include <iostream>
#include <io.h>
#include <fcntl.h>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>

static const int default_port = 10077;

namespace asio = boost::asio;
namespace po = boost::program_options;
namespace serialization = unreal_debugger::serialization;
using tcp = boost::asio::ip::tcp;

boost::asio::io_context ios;
std::unique_ptr<tcp::socket> sock;
serialization::message next_event;

namespace signals {
    signal line_received;
    signal watches_received;
    signal breakpoint_hit;
    signal user_watches_received;
}

Debugger debugger;
bool log_enabled;
std::shared_ptr<dap::Writer> log_file;
serialization::locked_message_queue send_queue;

std::vector<fs::path> source_roots;
int debug_port;


// Schedule an async receive of the next event from the debugger interface.
void receive_next_event()
{
    {
        boost::asio::async_read(*sock, boost::asio::buffer(&next_event.len_, 4), [](const boost::system::error_code& ec, std::size_t len) {
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
            next_event.buf_ = std::make_unique<char[]>(next_event.len_);

            boost::asio::async_read(*sock, boost::asio::buffer(next_event.buf_.get(), next_event.len_), [](const boost::system::error_code& ec, std::size_t len) {
                if (ec)
                {
                    dap::writef(log_file, "receiving event body error: %s", ec.message().c_str());
                    debugger_terminated();
                    return;
                }

                if (len != next_event.len_)
                {
                    dap::writef(log_file, "failed to read next message body: received %z of %z bytes\n", len, next_event.len_);
                    debugger_terminated();
                    return;
                }

                dispatch_event(next_event);
                next_event.buf_.reset();
                next_event.len_ = 0;
                receive_next_event();
            });
        });
    }
}

void send_next_message()
{
    auto&& next_msg = send_queue.top();

    // Begin the async send of the front-most message
    async_write(*sock, asio::buffer(&next_msg.len_, 4), [](const boost::system::error_code& ec, std::size_t n) {
        if (ec)
        {
            dap::writef(log_file, "sending command header received error: %s", ec.message().c_str());
        }

        if (n != 4)
        {
            dap::writef(log_file, "sending command header truncated: wrote %z of 4 bytes", n);
        }

        // Now send the message body
        auto&& next_msg = send_queue.top();
        async_write(*sock, asio::buffer(next_msg.buf_.get(), next_msg.len_), [len = next_msg.len_](const boost::system::error_code& ec, std::size_t n) {
            // Perform some basic error checking and log the results.
            if (ec)
            {
                dap::writef(log_file, "sending command received error: %s", ec.message().c_str());
            }

            if (n != len)
            {
                dap::writef(log_file, "sending command truncated: wrote %z of %z bytes", n, len);
            }

            // If the queue was not empty after removing this just-sent message, schedule the async send of the next message in the queue.
            // If the queue was empty the send will be scheduled by the next command that gets enqueued via send_command.
            if (!send_queue.pop())
            {
                send_next_message();
            }
        });
    });
}

// Enqueue the given command to send to the debugger interface.
void send_command(const commands::command& cmd)
{
    unreal_debugger::serialization::message msg = cmd.serialize();

    // If the queue was empty before we added this message, begin the async send.
    if (send_queue.push(std::move(msg)))
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
    po::options_description desc("Options");
    desc.add_options()
        ("help,h", "show usage")
        ("debug_port,d", po::value<int>(&debug_port)->default_value(0), "listen for client connections on <port> instead of stdin/stdout")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        std::cout << desc << std::endl;
        return 0;
    }

    if (debug_port > 0)
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

    if (debug_port > 0)
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

    if (debug_port > 0)
    {
        stop_debug_server();
    }
    log_file->close();
    stop_adapter();
}

