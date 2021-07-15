// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <chrono>
#include <optional>

namespace VideoCore {
class ShaderNotify {
public:
    [[nodiscard]] int ShadersBuilding() noexcept;

    void MarkShaderComplete() noexcept {
        ++num_complete;
    }

    void MarkShaderBuilding() noexcept {
        ++num_building;
    }

private:
    std::atomic_int num_building{};
    std::atomic_int num_complete{};
    int report_base{};

    bool completed{};
    int num_when_completed{};
    std::chrono::high_resolution_clock::time_point complete_time;
};
} // namespace VideoCore
