// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <variant>
#include <boost/container/static_vector.hpp>

#include "common/assert.h"
#include "common/logging/log.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

VKUpdateDescriptorQueue::VKUpdateDescriptorQueue(const Device& device_, VKScheduler& scheduler_)
    : device{device_}, scheduler{scheduler_} {
    payload_cursor = payload.data();
}

VKUpdateDescriptorQueue::~VKUpdateDescriptorQueue() = default;

void VKUpdateDescriptorQueue::TickFrame() {
    payload_cursor = payload.data();
}

void VKUpdateDescriptorQueue::Acquire() {
    // Minimum number of entries required.
    // This is the maximum number of entries a single draw call migth use.
    static constexpr size_t MIN_ENTRIES = 0x400;

    if (std::distance(payload.data(), payload_cursor) + MIN_ENTRIES >= payload.max_size()) {
        LOG_WARNING(Render_Vulkan, "Payload overflow, waiting for worker thread");
        scheduler.WaitWorker();
        payload_cursor = payload.data();
    }
    upload_start = payload_cursor;
}

} // namespace Vulkan
