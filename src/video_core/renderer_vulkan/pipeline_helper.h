// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>

#include <boost/container/small_vector.hpp>

#include "common/assert.h"
#include "common/common_types.h"
#include "shader_recompiler/shader_info.h"
#include "video_core/renderer_vulkan/vk_texture_cache.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/texture_cache/texture_cache.h"
#include "video_core/texture_cache/types.h"
#include "video_core/textures/texture.h"

namespace Vulkan {

struct TextureHandle {
    explicit TextureHandle(u32 data, bool via_header_index) {
        [[likely]] if (via_header_index) {
            image = data;
            sampler = data;
        }
        else {
            const Tegra::Texture::TextureHandle handle{data};
            image = handle.tic_id;
            sampler = via_header_index ? image : handle.tsc_id.Value();
        }
    }

    u32 image;
    u32 sampler;
};

class DescriptorLayoutBuilder {
public:
    DescriptorLayoutBuilder(const vk::Device& device_) : device{&device_} {}

    vk::DescriptorSetLayout CreateDescriptorSetLayout() const {
        if (bindings.empty()) {
            return nullptr;
        }
        return device->CreateDescriptorSetLayout({
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .bindingCount = static_cast<u32>(bindings.size()),
            .pBindings = bindings.data(),
        });
    }

    vk::DescriptorUpdateTemplateKHR CreateTemplate(VkDescriptorSetLayout descriptor_set_layout,
                                                   VkPipelineLayout pipeline_layout) const {
        if (entries.empty()) {
            return nullptr;
        }
        return device->CreateDescriptorUpdateTemplateKHR({
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .descriptorUpdateEntryCount = static_cast<u32>(entries.size()),
            .pDescriptorUpdateEntries = entries.data(),
            .templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET_KHR,
            .descriptorSetLayout = descriptor_set_layout,
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .pipelineLayout = pipeline_layout,
            .set = 0,
        });
    }

    vk::PipelineLayout CreatePipelineLayout(VkDescriptorSetLayout descriptor_set_layout) const {
        return device->CreatePipelineLayout({
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = descriptor_set_layout ? 1U : 0U,
            .pSetLayouts = bindings.empty() ? nullptr : &descriptor_set_layout,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr,
        });
    }

    void Add(const Shader::Info& info, VkShaderStageFlags stage) {
        for ([[maybe_unused]] const auto& desc : info.constant_buffer_descriptors) {
            Add(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stage);
        }
        for ([[maybe_unused]] const auto& desc : info.storage_buffers_descriptors) {
            Add(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stage);
        }
        for ([[maybe_unused]] const auto& desc : info.texture_buffer_descriptors) {
            Add(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, stage);
        }
        for ([[maybe_unused]] const auto& desc : info.texture_descriptors) {
            Add(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, stage);
        }
        for ([[maybe_unused]] const auto& desc : info.image_descriptors) {
            Add(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, stage);
        }
    }

private:
    void Add(VkDescriptorType type, VkShaderStageFlags stage) {
        bindings.push_back({
            .binding = binding,
            .descriptorType = type,
            .descriptorCount = 1,
            .stageFlags = stage,
            .pImmutableSamplers = nullptr,
        });
        entries.push_back(VkDescriptorUpdateTemplateEntryKHR{
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = type,
            .offset = offset,
            .stride = sizeof(DescriptorUpdateEntry),
        });
        ++binding;
        offset += sizeof(DescriptorUpdateEntry);
    }

    const vk::Device* device{};
    boost::container::small_vector<VkDescriptorSetLayoutBinding, 32> bindings;
    boost::container::small_vector<VkDescriptorUpdateTemplateEntryKHR, 32> entries;
    u32 binding{};
    size_t offset{};
};

inline void PushImageDescriptors(const Shader::Info& info, const VkSampler*& samplers,
                                 const ImageId*& image_view_ids, TextureCache& texture_cache,
                                 VKUpdateDescriptorQueue& update_descriptor_queue) {
    image_view_ids += info.texture_buffer_descriptors.size();
    for (const auto& desc : info.texture_descriptors) {
        const VkSampler sampler{*(samplers++)};
        ImageView& image_view{texture_cache.GetImageView(*(image_view_ids++))};
        const VkImageView vk_image_view{image_view.Handle(desc.type)};
        update_descriptor_queue.AddSampledImage(vk_image_view, sampler);
    }
    for (const auto& desc : info.image_descriptors) {
        ImageView& image_view{texture_cache.GetImageView(*(image_view_ids++))};
        if (desc.is_written) {
            texture_cache.MarkModification(image_view.image_id);
        }
        const VkImageView vk_image_view{image_view.StorageView(desc.type, desc.format)};
        update_descriptor_queue.AddImage(vk_image_view);
    }
}

} // namespace Vulkan
