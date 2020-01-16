#pragma once

namespace signals
{
    class signal {
    public:
        void wait()
        {
            std::unique_lock<std::mutex> lock(mutex);
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

    extern signal stack_changed;
}

