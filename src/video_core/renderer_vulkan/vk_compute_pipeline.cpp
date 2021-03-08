// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <vector>

#include <boost/container/small_vector.hpp>

#include "video_core/renderer_vulkan/vk_buffer_cache.h"
#include "video_core/renderer_vulkan/vk_compute_pipeline.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {
namespace {
vk::DescriptorSetLayout CreateDescriptorSetLayout(const Device& device, const Shader::Info& info) {
    boost::container::small_vector<VkDescriptorSetLayoutBinding, 24> bindings;
    u32 binding{};
    for ([[maybe_unused]] const auto& desc : info.constant_buffer_descriptors) {
        bindings.push_back({
            .binding = binding,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr,
        });
        ++binding;
    }
    for ([[maybe_unused]] const auto& desc : info.storage_buffers_descriptors) {
        bindings.push_back({
            .binding = binding,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr,
        });
        ++binding;
    }
    for (const auto& desc : info.texture_descriptors) {
        bindings.push_back({
            .binding = binding,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr,
        });
        ++binding;
    }
    return device.GetLogical().CreateDescriptorSetLayout({
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<u32>(bindings.size()),
        .pBindings = bindings.data(),
    });
}

vk::DescriptorUpdateTemplateKHR CreateDescriptorUpdateTemplate(
    const Device& device, const Shader::Info& info, VkDescriptorSetLayout descriptor_set_layout,
    VkPipelineLayout pipeline_layout) {
    boost::container::small_vector<VkDescriptorUpdateTemplateEntry, 24> entries;
    size_t offset{};
    u32 binding{};
    for ([[maybe_unused]] const auto& desc : info.constant_buffer_descriptors) {
        entries.push_back({
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .offset = offset,
            .stride = sizeof(DescriptorUpdateEntry),
        });
        ++binding;
        offset += sizeof(DescriptorUpdateEntry);
    }
    for ([[maybe_unused]] const auto& desc : info.storage_buffers_descriptors) {
        entries.push_back({
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .offset = offset,
            .stride = sizeof(DescriptorUpdateEntry),
        });
        ++binding;
        offset += sizeof(DescriptorUpdateEntry);
    }
    for (const auto& desc : info.texture_descriptors) {
        entries.push_back({
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .offset = offset,
            .stride = sizeof(DescriptorUpdateEntry),
        });
        ++binding;
        offset += sizeof(DescriptorUpdateEntry);
    }
    return device.GetLogical().CreateDescriptorUpdateTemplateKHR({
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .descriptorUpdateEntryCount = static_cast<u32>(entries.size()),
        .pDescriptorUpdateEntries = entries.data(),
        .templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET,
        .descriptorSetLayout = descriptor_set_layout,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE,
        .pipelineLayout = pipeline_layout,
        .set = 0,
    });
}

struct TextureHandle {
    explicit TextureHandle(u32 data, bool via_header_index) {
        const Tegra::Texture::TextureHandle handle{data};
        image = handle.tic_id;
        sampler = via_header_index ? image : handle.tsc_id.Value();
    }

    u32 image;
    u32 sampler;
};

VideoCommon::ImageViewType CastType(Shader::TextureType type) {
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
}
} // Anonymous namespace

ComputePipeline::ComputePipeline(const Device& device, VKDescriptorPool& descriptor_pool,
                                 VKUpdateDescriptorQueue& update_descriptor_queue_,
                                 const Shader::Info& info_, vk::ShaderModule spv_module_)
    : update_descriptor_queue{&update_descriptor_queue_}, info{info_},
      spv_module(std::move(spv_module_)),
      descriptor_set_layout(CreateDescriptorSetLayout(device, info)),
      descriptor_allocator(descriptor_pool, *descriptor_set_layout),
      pipeline_layout{device.GetLogical().CreatePipelineLayout({
          .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .setLayoutCount = 1,
          .pSetLayouts = descriptor_set_layout.address(),
          .pushConstantRangeCount = 0,
          .pPushConstantRanges = nullptr,
      })},
      descriptor_update_template{
          CreateDescriptorUpdateTemplate(device, info, *descriptor_set_layout, *pipeline_layout)},
      pipeline{device.GetLogical().CreateComputePipeline({
          .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .stage{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .pNext = nullptr,
              .flags = 0,
              .stage = VK_SHADER_STAGE_COMPUTE_BIT,
              .module = *spv_module,
              .pName = "main",
              .pSpecializationInfo = nullptr,
          },
          .layout = *pipeline_layout,
          .basePipelineHandle = 0,
          .basePipelineIndex = 0,
      })} {}

void ComputePipeline::ConfigureBufferCache(BufferCache& buffer_cache) {
    buffer_cache.SetEnabledComputeUniformBuffers(info.constant_buffer_mask);
    buffer_cache.UnbindComputeStorageBuffers();
    size_t index{};
    for (const auto& desc : info.storage_buffers_descriptors) {
        ASSERT(desc.count == 1);
        buffer_cache.BindComputeStorageBuffer(index, desc.cbuf_index, desc.cbuf_offset, true);
        ++index;
    }
    buffer_cache.UpdateComputeBuffers();
    buffer_cache.BindHostComputeBuffers();
}

void ComputePipeline::ConfigureTextureCache(Tegra::Engines::KeplerCompute& kepler_compute,
                                            Tegra::MemoryManager& gpu_memory,
                                            TextureCache& texture_cache) {
    texture_cache.SynchronizeComputeDescriptors();

    static constexpr size_t max_elements = 64;
    std::array<ImageId, max_elements> image_view_ids;
    boost::container::static_vector<u32, max_elements> image_view_indices;
    boost::container::static_vector<VkSampler, max_elements> sampler_handles;

    const auto& launch_desc{kepler_compute.launch_description};
    const auto& cbufs{launch_desc.const_buffer_config};
    const bool via_header_index{launch_desc.linked_tsc};
    for (const auto& desc : info.texture_descriptors) {
        const u32 cbuf_index{desc.cbuf_index};
        const u32 cbuf_offset{desc.cbuf_offset};
        ASSERT(((launch_desc.const_buffer_enable_mask >> cbuf_index) & 1) != 0);

        const GPUVAddr addr{cbufs[cbuf_index].Address() + cbuf_offset};
        const u32 raw_handle{gpu_memory.Read<u32>(addr)};

        const TextureHandle handle(raw_handle, via_header_index);
        image_view_indices.push_back(handle.image);

        Sampler* const sampler = texture_cache.GetComputeSampler(handle.sampler);
        sampler_handles.push_back(sampler->Handle());
    }

    const std::span indices_span(image_view_indices.data(), image_view_indices.size());
    texture_cache.FillComputeImageViews(indices_span, image_view_ids);

    size_t index{};
    for (const auto& desc : info.texture_descriptors) {
        const VkSampler vk_sampler{sampler_handles[index]};
        ImageView& image_view{texture_cache.GetImageView(image_view_ids[index])};
        const VkImageView vk_image_view{image_view.Handle(CastType(desc.type))};
        update_descriptor_queue->AddSampledImage(vk_image_view, vk_sampler);
        ++index;
    }
}

VkDescriptorSet ComputePipeline::UpdateDescriptorSet() {
    const VkDescriptorSet descriptor_set{descriptor_allocator.Commit()};
    update_descriptor_queue->Send(*descriptor_update_template, descriptor_set);
    return descriptor_set;
}

} // namespace Vulkan
