
// Simple signal handling for the DAP debugger.
class Signal
{
public:
    void wait()
    {
        std::unique_lock<std::mutex> lock(mu_);
        cv_.wait(lock, [&] { return fired_; });
    }

    void fire()
    {
        std::unique_lock<std::mutex> lock(mu_);
        fired_ = true;
        cv_.notify_all();
    }

private:
    std::mutex mu_;
    std::condition_variable cv_;
    bool fired_ = false;
};

extern Signal term;

void start_debug_server();
void start_debug_local();
void stop_adapter();

// Debugger events to send to the debug client.
void breakpoint_hit();
void console_message(const std::string& msg);


