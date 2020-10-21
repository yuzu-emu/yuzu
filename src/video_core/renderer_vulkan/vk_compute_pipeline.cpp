// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <vector>

#include "video_core/renderer_vulkan/vk_compute_pipeline.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_decompiler.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

VKComputePipeline::VKComputePipeline(const VKDevice& device, VKScheduler& scheduler,
                                     VKDescriptorPool& descriptor_pool,
                                     VKUpdateDescriptorQueue& update_descriptor_queue,
                                     const SPIRVShader& shader)
    : device{device}, scheduler{scheduler}, entries{shader.entries},
      descriptor_set_layout{CreateDescriptorSetLayout()},
      descriptor_allocator{descriptor_pool, *descriptor_set_layout},
      update_descriptor_queue{update_descriptor_queue}, layout{CreatePipelineLayout()},
      descriptor_template{CreateDescriptorUpdateTemplate()},
      shader_module{CreateShaderModule(shader.code)}, pipeline{CreatePipeline()} {}

VKComputePipeline::~VKComputePipeline() = default;

VkDescriptorSet VKComputePipeline::CommitDescriptorSet() {
    if (!descriptor_template) {
        return {};
    }
    const VkDescriptorSet set = descriptor_allocator.Commit();
    update_descriptor_queue.Send(*descriptor_template, set);
    return set;
}

vk::DescriptorSetLayout VKComputePipeline::CreateDescriptorSetLayout() const {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    u32 binding = 0;
    const auto add_bindings = [&](VkDescriptorType descriptor_type, std::size_t num_entries) {
        // TODO(Rodrigo): Maybe make individual bindings here?
        for (u32 bindpoint = 0; bindpoint < static_cast<u32>(num_entries); ++bindpoint) {
            bindings.push_back({
                .binding = binding++,
                .descriptorType = descriptor_type,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .pImmutableSamplers = nullptr,
            });
        }
    };
    add_bindings(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, entries.const_buffers.size());
    add_bindings(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, entries.global_buffers.size());
    add_bindings(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, entries.uniform_texels.size());
    add_bindings(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, entries.samplers.size());
    add_bindings(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, entries.storage_texels.size());
    add_bindings(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, entries.images.size());

    return device.GetLogical().CreateDescriptorSetLayout({
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<u32>(bindings.size()),
        .pBindings = bindings.data(),
    });
}

vk::PipelineLayout VKComputePipeline::CreatePipelineLayout() const {
    return device.GetLogical().CreatePipelineLayout({
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = descriptor_set_layout.address(),
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr,
    });
}

vk::DescriptorUpdateTemplateKHR VKComputePipeline::CreateDescriptorUpdateTemplate() const {
    std::vector<VkDescriptorUpdateTemplateEntryKHR> template_entries;
    u32 binding = 0;
    u32 offset = 0;
    FillDescriptorUpdateTemplateEntries(entries, binding, offset, template_entries);
    if (template_entries.empty()) {
        // If the shader doesn't use descriptor sets, skip template creation.
        return {};
    }

    return device.GetLogical().CreateDescriptorUpdateTemplateKHR({
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .descriptorUpdateEntryCount = static_cast<u32>(template_entries.size()),
        .pDescriptorUpdateEntries = template_entries.data(),
        .templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET_KHR,
        .descriptorSetLayout = *descriptor_set_layout,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .pipelineLayout = *layout,
        .set = DESCRIPTOR_SET,
    });
}

vk::ShaderModule VKComputePipeline::CreateShaderModule(const std::vector<u32>& code) const {
    device.SaveShader(code);

    return device.GetLogical().CreateShaderModule({
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = code.size() * sizeof(u32),
        .pCode = code.data(),
    });
}

vk::Pipeline VKComputePipeline::CreatePipeline() const {

    VkComputePipelineCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage =
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .vkModule = *shader_module,
                .pName = "main",
                .pSpecializationInfo = nullptr,
            },
        .layout = *layout,
        .basePipelineHandle = nullptr,
        .basePipelineIndex = 0,
    };

    const VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT subgroup_size_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT,
        .pNext = nullptr,
        .requiredSubgroupSize = GuestWarpSize,
    };

    if (entries.uses_warps && device.IsGuestWarpSizeSupported(VK_SHADER_STAGE_COMPUTE_BIT)) {
        ci.stage.pNext = &subgroup_size_ci;
    }

    return device.GetLogical().CreateComputePipeline(ci);
}

} // namespace Vulkan
