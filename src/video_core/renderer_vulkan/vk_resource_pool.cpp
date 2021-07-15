// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <optional>

#include "video_core/renderer_vulkan/vk_master_semaphore.h"
#include "video_core/renderer_vulkan/vk_resource_pool.h"

namespace Vulkan {

ResourcePool::ResourcePool(MasterSemaphore& master_semaphore_, size_t grow_step_)
    : master_semaphore{&master_semaphore_}, grow_step{grow_step_} {}

size_t ResourcePool::CommitResource() {
    // Refresh semaphore to query updated results
    master_semaphore->Refresh();
    const u64 gpu_tick = master_semaphore->KnownGpuTick();
    const auto search = [this, gpu_tick](size_t begin, size_t end) -> std::optional<size_t> {
        for (size_t iterator = begin; iterator < end; ++iterator) {
            if (gpu_tick >= ticks[iterator]) {
                ticks[iterator] = master_semaphore->CurrentTick();
                return iterator;
            }
        }
        return std::nullopt;
    };
    // Try to find a free resource from the hinted position to the end.
    std::optional<size_t> found = search(hint_iterator, ticks.size());
    if (!found) {
        // Search from beginning to the hinted position.
        found = search(0, hint_iterator);
        if (!found) {
            // Both searches failed, the pool is full; handle it.
            const size_t free_resource = ManageOverflow();

            ticks[free_resource] = master_semaphore->CurrentTick();
            found = free_resource;
        }
    }
    // Free iterator is hinted to the resource after the one that's been commited.
    hint_iterator = (*found + 1) % ticks.size();
    return *found;
}

size_t ResourcePool::ManageOverflow() {
    const size_t old_capacity = ticks.size();
    Grow();

    // The last entry is guaranted to be free, since it's the first element of the freshly
    // allocated resources.
    return old_capacity;
}

void ResourcePool::Grow() {
    const size_t old_capacity = ticks.size();
    ticks.resize(old_capacity + grow_step);
    Allocate(old_capacity, old_capacity + grow_step);
}

} // namespace Vulkan
