// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include <queue>

#include "common/unique_function.h"

namespace Common {

class ThreadWorker final {
public:
    explicit ThreadWorker(std::size_t num_workers, const std::string& name);
    ~ThreadWorker();
    void QueueWork(UniqueFunction<void> work);
    void WaitForRequests();

private:
    std::vector<std::thread> threads;
    std::queue<UniqueFunction<void>> requests;
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::condition_variable wait_condition;
    std::atomic_bool stop{};
};

} // namespace Common
