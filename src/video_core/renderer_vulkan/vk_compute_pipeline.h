// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;
class VKScheduler;
class VKUpdateDescriptorQueue;

class ComputePipeline {
public:
    explicit ComputePipeline();
    ~ComputePipeline();
};

} // namespace Vulkan
