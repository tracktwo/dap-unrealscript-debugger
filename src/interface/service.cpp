// service.cpp
//

#include <thread>
#include <boost/asio.hpp>

#include "service.h"

std::unique_ptr<DebuggerService> service;
boost::asio::io_context ios;
std::thread worker;
std::atomic<service_state> state;

namespace asio = boost::asio;

static const int default_port = 10077;

DebuggerService::DebuggerService() :
    watch_indices_{1, 1, 1}
{}

void DebuggerService::start()
{
    // TODO Add environment var for port override?
    int port = default_port;

    // Create the acceptor to listen for connections.
    acceptor_ = std::make_unique<tcp::acceptor>(ios, tcp::endpoint(tcp::v4(), port));

    // We are now in the disconnected state: the debugger service is up and running, but not yet
    // connected.
    state = service_state::disconnected;

    // Queue the next connection
    accept_connection();
}

// Request a stop, usually because of an error.
//
// We just set a flag, which will be tested the next time we enter the API from Unreal.
// (note that this flag is an atomic). In the stopped state the debugger will attempt to
// cleanly shutdown all of its state (including closing the socket and halting the IO
// thread) and then restart itself.
void DebuggerService::stop()
{
    state = service_state::stopped;
}

// Shutdown the debugger with no restart. This is intended to be called when Unreal initiates
// a debugger shutdown (via a toggledebugger console command).
void DebuggerService::shutdown()
{
    // Send a 'terminated' event to the debugger client so it knows unreal has stopped the debugger.
    send_event(events::terminated{});
    state = service_state::shutdown;
}

// Log an error message to the console and stop the current debugger.
void DebuggerService::fatal_error(const char* msg, ...)
{
    va_list args;
    printf("Debugger Fatal Error: ");
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
    stop();
}

// Receive and process the next command message from the client. Since there is only ever a single IO thread reading
// these messages access to the next message object does not need to be synchronized.
void DebuggerService::receive_next_message()
{
    // Register a handler to read the command header.
    async_read(*socket_, boost::asio::buffer(&next_command_.len_, 4), [this](const boost::system::error_code& ec, std::size_t len) {
        if (ec)
        {
            fatal_error("Receiving command header error: %s\n", ec.message().c_str());
            return;
        }
        if (len != 4)
        {
            fatal_error("Failed to read header.");
            return;
        }

        // Allocate the buffer for the command
        // TODO: Keep any previous buffer here and re-use it if it's big enough instead of re-allocating each read.
        next_command_.buf_ = std::make_unique<char[]>(next_command_.len_);

        // Register a handler to read the command body
        async_read(*socket_, boost::asio::buffer(next_command_.buf_.get(), next_command_.len_), [this](const boost::system::error_code& ec, std::size_t len) {
            if (ec)
            {
                printf("Receiving command body error: %s\n", ec.message().c_str());
                return;
            }
            if (len != next_command_.len_)
            {
                printf("Failed to read body: read %z of %z bytes\n", len, next_command_.len_);
                return;
            }

            // Dispatch the command. This must happen on the same IO thread and complete before we return so we can re-use the message space.
            dispatch_command(next_command_);

            // Clear out the buffer.
            next_command_.buf_ = {};
            next_command_.len_ = 0;

            // Start async handling for the next message.
            receive_next_message();
        });
    });
}

// Asynchronously wait for the next connection.
void DebuggerService::accept_connection()
{
    acceptor_->async_accept([this](const boost::system::error_code& ec, tcp::socket socket) {
        // We have a new connection.
        this->socket_ = std::make_unique<tcp::socket>(std::move(socket));
        state = service_state::connected;
        this->receive_next_message();
    });
}

// Enqueues a message to send to the debugger client. If the queue is
// currently empty it will also initiate an async send of the message.
void DebuggerService::send_event(const events::event& ev)
{
    bool is_empty = false;

    // Serialize and enqueue the next message. If the queue was empty prior to the message
    // we just enqueued, register a handler to send this message. This actual send will not be serviced
    // on this thread, but on the IO thread.
    if (send_queue_.push(ev.serialize()))
    {
        send_next_message();
    }
}

// Send the next message over the wire via an async send. The completion handler for
// this send will schedule the sending of the next message if the queue is not empty
// when it completes.
void DebuggerService::send_next_message()
{
    // This must be on the single IO writer thread, so nobody else can be emptying the queue
    // while we're processing this message. We don't need to lock access to this top element while
    // we're processing the send.
    auto&& next_msg = send_queue_.top();

    // Start the async send of the header for this message.
    async_write(*socket_, boost::asio::buffer(&next_msg.len_, 4), [this](const boost::system::error_code& ec, std::size_t n) {
        if (ec)
        {
            fatal_error("Sending event header error: %s\n", ec.message().c_str());
            return;
        }
        if (n != 4)
        {
            fatal_error("Sending event header truncated: wrote %z of 4 bytes\n", n);
            return;
        }

        // Fetch the top element again so we can send the body
        auto&& next_msg = send_queue_.top();

        // Start the async send of the message body
        async_write(*socket_, boost::asio::buffer(next_msg.buf_.get(), next_msg.len_), [this, len = next_msg.len_](boost::system::error_code ec, std::size_t n) {
            if (ec)
            {
                fatal_error("Sending event body error: %s\n", ec.message().c_str());
                return;
            }
            if (n != len)
            {
                fatal_error("Sending event body truncated: wrote %z of %z bytes\n", n, len);
                return;
            }

            // This message is now complete, pop it from the queue. If the queue is not empty after this pop, register
            // a new send. Note that the test for emptiness is done while the internal lock is held while popping the
            // element, so there is no race here with the producer thread: if the queue is empty we must just return.
            // Nobody can be yet adding anything to the queue, and any thread blocked on the lock must observe the
            // empty queue and will register the next send handler themselves.
            if (!send_queue_.pop())
            {
                send_next_message();
            }
        });
    });

}

void worker_loop()
{
    // The main worker loop thread just services ASIO tasks.
    ios.run();
}

// Start the debugger service: create the service instance
void start_debugger_service()
{
    if (service)
        service.release();

    service = std::make_unique<DebuggerService>();

    // Start listening for connections.
    service->start();

    // run the worker thread to service i/o
    worker = std::thread(worker_loop);
}

// Try to ensure the debugger service is in a good state. Returns 'true' if the service is up
// and we can service events, or false otherwise. A false result may mean the the service is either
// shut down or in the process of shutting down, but no debugger API calls can be serviced.
bool check_service()
{
    switch (state)
    {
    case service_state::stopped:
    case service_state::shutdown:
        // If we are stopped or shut down and there is a service, kill the service (as cleanly as possible)
        // and for the 'stopped' case, restart it.
        if (service)
        {
            // Stop the ASIO task service. This will allow the worker thread to halt.
            ios.stop();

            // Stop the worker thread. If we ended up calling stop from a thread owned by
            // Unreal, we can just join (and this shouldn't take *too* long for the worker
            // thread to stop). If we're called on the worker thread then all we can do is detach
            // to let it unwind itself and stop (which it should do quickly, now that the io
            // context is stopped).
            if (worker.joinable() && std::this_thread::get_id() != worker.get_id())
            {
                worker.join();
            }
            else
            {
                worker.detach();
            }

            // The worker thread has cleanly shut down (or we have detached it and can't do anything more).
            // Destroy the service.
            service.release();

            callback_function = nullptr;

            printf("Debugger stopped!\n");
        }

        // Restart, if applicable
        if (state == service_state::stopped)
        {
            start_debugger_service();
            printf("Debugger service running!\n");
        }
        // Return false: even though the service is possibly now running (if we restarted from 'stopped') it is not connected.
        return false;

    case service_state::disconnected:
        // In the disconnected state the service is healthy but cannot yet do anything. Return false but otherwise don't
        // take any action on the service.
        return false;

    case service_state::connected:
        // The service is healthy and connected and can service commands and events.
        return true;

    default:
        printf("Debugger error: Unknown state %d\n", static_cast<int>(state.load()));
        state = service_state::stopped;
        return check_service();
    }
}

