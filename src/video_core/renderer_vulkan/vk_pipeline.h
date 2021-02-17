// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>

#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Pipeline {
public:
    /// Add a reference count to the pipeline
    void AddRef() noexcept {
        ++ref_count;
    }

    [[nodiscard]] bool RemoveRef() noexcept {
        --ref_count;
        return ref_count == 0;
    }

    [[nodiscard]] u64 UsageTick() const noexcept {
        return usage_tick;
    }

protected:
    u64 usage_tick{};

private:
    size_t ref_count{};
};

} // namespace Vulkan
