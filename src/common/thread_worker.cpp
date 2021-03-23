// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/thread.h"
#include "common/thread_worker.h"

namespace Common {

ThreadWorker::ThreadWorker(std::size_t num_workers, const std::string& name) {
    for (std::size_t i = 0; i < num_workers; ++i)
        threads.emplace_back([this, thread_name{std::string{name}}] {
            Common::SetCurrentThreadName(thread_name.c_str());

            // Wait for first request
            {
                std::unique_lock lock{queue_mutex};
                condition.wait(lock, [this] { return stop || !requests.empty(); });
            }

            while (true) {
                std::function<void()> task;

                {
                    std::unique_lock lock{queue_mutex};
                    condition.wait(lock, [this] { return stop || !requests.empty(); });
                    if (stop || requests.empty()) {
                        return;
                    }
                    task = std::move(requests.front());
                    requests.pop();

                    if (requests.empty()) {
                        wait_condition.notify_one();
                    }
                }

                task();
            }
        });
}

ThreadWorker::~ThreadWorker() {
    {
        std::unique_lock lock{queue_mutex};
        stop = true;
    }
    condition.notify_all();
    for (std::thread& thread : threads) {
        thread.join();
    }
}

void ThreadWorker::QueueWork(std::function<void()>&& work) {
    {
        std::unique_lock lock{queue_mutex};
        requests.emplace(work);
    }
    condition.notify_one();
}

void ThreadWorker::WaitForRequests() {
    std::unique_lock lock{queue_mutex};
    wait_condition.wait(lock, [this] { return stop || requests.empty(); });
}

} // namespace Common
