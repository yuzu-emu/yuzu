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
        } else {
            const Tegra::Texture::TextureHandle handle{data};
            image = handle.tic_id;
            sampler = via_header_index ? image : handle.tsc_id.Value();
        }
    }

    u32 image;
    u32 sampler;
};

struct DescriptorLayoutTuple {
    vk::DescriptorSetLayout descriptor_set_layout;
    vk::PipelineLayout pipeline_layout;
    vk::DescriptorUpdateTemplateKHR descriptor_update_template;
};

class DescriptorLayoutBuilder {
public:
    DescriptorLayoutTuple Create(const vk::Device& device) {
        DescriptorLayoutTuple result;
        if (!bindings.empty()) {
            result.descriptor_set_layout = device.CreateDescriptorSetLayout({
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .bindingCount = static_cast<u32>(bindings.size()),
                .pBindings = bindings.data(),
            });
        }
        result.pipeline_layout = device.CreatePipelineLayout({
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = result.descriptor_set_layout ? 1U : 0U,
            .pSetLayouts = bindings.empty() ? nullptr : result.descriptor_set_layout.address(),
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr,
        });
        if (!entries.empty()) {
            result.descriptor_update_template = device.CreateDescriptorUpdateTemplateKHR({
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO_KHR,
                .pNext = nullptr,
                .flags = 0,
                .descriptorUpdateEntryCount = static_cast<u32>(entries.size()),
                .pDescriptorUpdateEntries = entries.data(),
                .templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET_KHR,
                .descriptorSetLayout = *result.descriptor_set_layout,
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .pipelineLayout = *result.pipeline_layout,
                .set = 0,
            });
        }
        return result;
    }

    void Add(const Shader::Info& info, VkShaderStageFlags stage) {
        for ([[maybe_unused]] const auto& desc : info.constant_buffer_descriptors) {
            Add(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stage);
        }
        for ([[maybe_unused]] const auto& desc : info.storage_buffers_descriptors) {
            Add(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stage);
        }
        for ([[maybe_unused]] const auto& desc : info.texture_descriptors) {
            Add(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, stage);
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

    boost::container::small_vector<VkDescriptorSetLayoutBinding, 32> bindings;
    boost::container::small_vector<VkDescriptorUpdateTemplateEntryKHR, 32> entries;
    u32 binding{};
    size_t offset{};
};

inline VideoCommon::ImageViewType CastType(Shader::TextureType type) {
    switch (type) {
    case Shader::TextureType::Color1D:
    case Shader::TextureType::Shadow1D:
        return VideoCommon::ImageViewType::e1D;
    case Shader::TextureType::ColorArray1D:
    case Shader::TextureType::ShadowArray1D:
        return VideoCommon::ImageViewType::e1DArray;
    case Shader::TextureType::Color2D:
    case Shader::TextureType::Shadow2D:
        return VideoCommon::ImageViewType::e2D;
    case Shader::TextureType::ColorArray2D:
    case Shader::TextureType::ShadowArray2D:
        return VideoCommon::ImageViewType::e2DArray;
    case Shader::TextureType::Color3D:
    case Shader::TextureType::Shadow3D:
        return VideoCommon::ImageViewType::e3D;
    case Shader::TextureType::ColorCube:
    case Shader::TextureType::ShadowCube:
        return VideoCommon::ImageViewType::Cube;
    case Shader::TextureType::ColorArrayCube:
    case Shader::TextureType::ShadowArrayCube:
        return VideoCommon::ImageViewType::CubeArray;
    }
    UNREACHABLE_MSG("Invalid texture type {}", type);
    return {};
}

inline void PushImageDescriptors(const Shader::Info& info, const VkSampler* samplers,
                                 const ImageId* image_view_ids, TextureCache& texture_cache,
                                 VKUpdateDescriptorQueue& update_descriptor_queue, size_t& index) {
    for (const auto& desc : info.texture_descriptors) {
        const VkSampler sampler{samplers[index]};
        ImageView& image_view{texture_cache.GetImageView(image_view_ids[index])};
        const VkImageView vk_image_view{image_view.Handle(CastType(desc.type))};
        update_descriptor_queue.AddSampledImage(vk_image_view, sampler);
        ++index;
    }
}

} // namespace Vulkan
