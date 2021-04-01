// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/thread.h"
#include "common/thread_worker.h"

namespace Common {

ThreadWorker::ThreadWorker(std::size_t num_workers, const std::string& name) {
    const auto lambda = [this, thread_name{std::string{name}}] {
        Common::SetCurrentThreadName(thread_name.c_str());

        while (!stop) {
            UniqueFunction<void> task;
            {
                std::unique_lock lock{queue_mutex};
                if (requests.empty()) {
                    wait_condition.notify_all();
                }
                condition.wait(lock, [this] { return stop || !requests.empty(); });
                if (stop || requests.empty()) {
                    break;
                }
                task = std::move(requests.front());
                requests.pop();
            }
            task();
        }
        wait_condition.notify_all();
    };
    for (size_t i = 0; i < num_workers; ++i) {
        threads.emplace_back(lambda);
    }
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

void ThreadWorker::QueueWork(UniqueFunction<void> work) {
    {
        std::unique_lock lock{queue_mutex};
        requests.emplace(std::move(work));
    }
    condition.notify_one();
}

void ThreadWorker::WaitForRequests() {
    std::unique_lock lock{queue_mutex};
    wait_condition.wait(lock, [this] { return stop || requests.empty(); });
}

} // namespace Common
