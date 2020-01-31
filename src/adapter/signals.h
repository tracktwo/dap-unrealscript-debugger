#pragma once

namespace unreal_debugger::client::signals
{
    class signal {
    public:
        void wait()
        {
            std::unique_lock<std::mutex> lock(mutex);
            if (fired)
                return;
            cv.wait(lock, [this] { return fired; });
        }

        void fire()
        {
            std::unique_lock<std::mutex> lock(mutex);
            fired = true;
            cv.notify_all();
        }

        void reset()
        {
            std::unique_lock<std::mutex> lock(mutex);
            fired = false;
        }

    private:
        std::mutex mutex;
        std::condition_variable cv;
        bool fired = false;
    };

    extern signal line_received;
    extern signal watches_received;
    extern signal breakpoint_hit;
    extern signal user_watches_received;
    extern signal breakpoint_added;
}

