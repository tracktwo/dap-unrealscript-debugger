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

    acceptor_ = std::make_unique<tcp::acceptor>(ios, tcp::endpoint(tcp::v4(), port));

    // Queue the next connection
    accept_connection();
    state = service_state::running;
}

void DebuggerService::stop()
{
    // Request a stop, usually because of an error or if the client debug session has stopped.
    // We just set a flag, which will be tested the next time we enter the API from Unreal.
    // (note that this flag is an atomic).
    state = service_state::stopped;
}

void DebuggerService::receive_next_message()
{
    {
        Lock lock{mu_};
        socket_->async_receive(boost::asio::buffer(&next_message_.len, 4), [this](const boost::system::error_code& ec, std::size_t len) {
            if (ec)
            {
                printf("UnrealDebugger: Receiving command header error: %s\n", ec.message().c_str());
                stop();
                return;
            }
            if (len != 4)
            {
                printf("UnrealDebugger: failed to read header.");
                stop();
                return;
            }

            next_message_.bytes = std::make_unique<char[]>(next_message_.len);
            socket_->async_receive(boost::asio::buffer(next_message_.bytes.get(), next_message_.len), [this](const boost::system::error_code& ec, std::size_t len) {
                unreal_debugger::commands::Command cmd;

                if (ec)
                {
                    printf("UnrealDebugger: Receiving command body error: %s\n", ec.message().c_str());
                    stop();
                    return;
                }
                if (len != next_message_.len)
                {
                    printf("UnrealDebugger: failed to read body: read %z of %z bytes\n", len, next_message_.len);
                    stop();
                    return;
                }

                if (cmd.ParseFromArray(next_message_.bytes.get(), next_message_.len))
                {
                    dispatch_command(cmd);
                }
                else
                {
                    printf("UnrealDebugger: Failed to parse command.");
                }
                receive_next_message();
            });
        });
    }
}

void DebuggerService::accept_connection()
{
    acceptor_->async_accept([this](const boost::system::error_code& ec, tcp::socket socket) {
        // We have a new connection. Create a new websocket from the tcp socket, and start the upgrade handshake.
        this->socket_ = std::make_unique<tcp::socket>(std::move(socket));
        this->connected_ = true;
        this->receive_next_message();
    });
}

// Enqueues a message to send to the debugger client. If the queue is
// currently empty it will also initiate an async send of the message.
void DebuggerService::send_event(const unreal_debugger::events::Event& msg)
{
    bool is_empty = false;

    // FIXME
    if (!connected_)
        return;

    // Control the scoping of the lock guard while we access the send queue.
    {
        Lock lock(mu_);

        if (msg.kind() != unreal_debugger::events::Event_Kind_AddAWatch)
            printf("Sending event %s\n", unreal_debugger::events::Event::Kind_Name(msg.kind()).c_str());
        // Serialize the message into a buffer that we can send over the wire.
        std::size_t len = msg.ByteSizeLong();
        std::unique_ptr<char[]> buf = std::make_unique<char[]>(len);
        msg.SerializeToArray(buf.get(), len);

        is_empty = send_queue_.empty();
        // Add the message to the send queue
        send_queue_.emplace_back(std::move(buf), len);
    }

    // The queue was empty prior to the message we just enqueued, so send this message now.
    if (is_empty)
    {
        send_next_message();
    }
}

// Send the next message over the wire via an async send. The completion handler for
// this send will schedule the sending of the next message if the queue is not empty
// when it completes.
void DebuggerService::send_next_message()
{
    Lock lock(mu_);

    assert(!send_queue_.empty());
    auto&& next_msg = send_queue_.front();

    socket_->async_send(boost::asio::buffer(&next_msg.len, 4), [this](const boost::system::error_code& ec, std::size_t n) {
        if (ec)
        {
            printf("UnrealDebugger: Sending event header error: %s\n", ec.message().c_str());
            stop();
            return;
        }
        if (n != 4)
        {
            printf("UnrealDebugger: Sending event header truncated: wrote %z of 4 bytes\n", n);
            stop();
            return;
        }

        auto&& next_msg = send_queue_.front();

        socket_->async_send(boost::asio::buffer(next_msg.bytes.get(), next_msg.len), [this, len = next_msg.len](boost::system::error_code ec, std::size_t n) {
            bool is_empty = false;
            {
                Lock lock(mu_);

                if (ec)
                {
                    printf("UnrealDebugger: Sending event body error: %s\n", ec.message().c_str());
                    stop();
                    return;
                }
                if (n != len)
                {
                    printf("UnrealDebugger: Sending event body truncated: wrote %z of %z bytes\n", n, len);
                    stop();
                    return;
                }

                send_queue_.pop_front();
                is_empty = send_queue_.empty();
            }

            // If we have more data ready to go, send the next one.
            if (!is_empty) {
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

            printf("Debugger stopped!\n");
        }

        // Restart, if applicable
        if (state == service_state::stopped)
        {
            start_debugger_service();
            printf("Debugger service running!\n");
            return true;
        }
        return false;

    case service_state::running:
        return true;
    }
}

