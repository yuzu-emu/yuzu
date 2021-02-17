// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "common/common_types.h"

namespace Vulkan {

class MasterSemaphore;

/**
 * Handles a pool of resources protected by fences. Manages resource overflow allocating more
 * resources.
 */
class ResourcePool {
public:
    explicit ResourcePool() = default;
    explicit ResourcePool(MasterSemaphore& master_semaphore, size_t grow_step);

    virtual ~ResourcePool() = default;

    ResourcePool& operator=(ResourcePool&&) noexcept = default;
    ResourcePool(ResourcePool&&) noexcept = default;

    ResourcePool& operator=(const ResourcePool&) = default;
    ResourcePool(const ResourcePool&) = default;

protected:
    size_t CommitResource();

    /// Called when a chunk of resources have to be allocated.
    virtual void Allocate(size_t begin, size_t end) = 0;

private:
    /// Manages pool overflow allocating new resources.
    size_t ManageOverflow();

    /// Allocates a new page of resources.
    void Grow();

    MasterSemaphore* master_semaphore{};
    size_t grow_step = 0;     ///< Number of new resources created after an overflow
    size_t hint_iterator = 0; ///< Hint to where the next free resources is likely to be found
    std::vector<u64> ticks;   ///< Ticks for each resource
};

} // namespace Vulkan
