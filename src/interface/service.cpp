// service.cpp
//

#include <thread>
#include <boost/asio.hpp>

#include "service.h"

std::unique_ptr<DebuggerService> service;

namespace asio = boost::asio;

static const int default_port = 10077;

DebuggerService::DebuggerService() :
    ios_{},
    socket_{ios_},
    watch_indices_{1, 1, 1}
{}

void DebuggerService::start()
{
    // TODO Add environment var for port override?
    int port = default_port;
    addr_ = "tcp://127.0.0.1:" + std::to_string(port);
    socket_.bind(addr_);

    worker_ = std::thread(&DebuggerService::main_loop, this);
}

void DebuggerService::stop()
{
    // Note that the io_context is documented to be thread-safe. Request a stop and
    // then halt the worker thread.
    socket_.unbind(addr_);
    ios_.stop();
    if (worker_.joinable())
        worker_.join();
}

void DebuggerService::receive_next_message()
{
    {
        Lock lock{mu_};
        socket_.async_receive([this](boost::system::error_code& ec, azmq::message& msg, std::size_t len) {
            {
                if (ec)
                {
                    printf("UnrealDebugger: Sending command received error: %s\n", ec.message().c_str());
                }

                unreal_debugger::commands::Command cmd;
                if (cmd.ParseFromArray(msg.data(), msg.size()))
                {
                    dispatch_command(cmd);
                }
                else
                {
                    printf("UnrealDebugger: Failed to parse command.");
                }
            }
            receive_next_message();
        });
    }
}

void DebuggerService::main_loop()
{
    receive_next_message();
    ios_.run();
}

// Enqueues a message to send to the debugger client. If the queue is
// currently empty it will also initiate an async send of the message.
void DebuggerService::send_event(const unreal_debugger::events::Event& msg)
{
    bool is_empty = false;

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

    socket_.async_send(next_msg.buffer, [this, len=next_msg.len](boost::system::error_code ec, std::size_t n) {

        bool is_empty = false;
        {
            Lock lock(mu_);

            if (ec)
            {
                printf("UnrealDebugger: Sending command received error: %s\n", ec.message().c_str());
            }
            if (n != len)
            {
                printf("UnrealDebugger: Sending command truncated: wrote %z of %z bytes\n", n, len);
            }

            send_queue_.pop_front();
            is_empty = send_queue_.empty();
        }

        // If we have more data ready to go, send the next one.
        if (!is_empty) {
            send_next_message();
        }
    });
}
