// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <compare>

#include "video_core/engines/fermi_2d.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/texture_cache/types.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

using VideoCommon::Region2D;

class Device;
class Framebuffer;
class ImageView;
class StateTracker;
class VKScheduler;

struct BlitImagePipelineKey {
    constexpr auto operator<=>(const BlitImagePipelineKey&) const noexcept = default;

    VkRenderPass renderpass;
    Tegra::Engines::Fermi2D::Operation operation;
};

class BlitImageHelper {
public:
    explicit BlitImageHelper(const Device& device, VKScheduler& scheduler,
                             StateTracker& state_tracker, DescriptorPool& descriptor_pool);
    ~BlitImageHelper();

    void BlitColor(const Framebuffer* dst_framebuffer, const ImageView& src_image_view,
                   const Region2D& dst_region, const Region2D& src_region,
                   Tegra::Engines::Fermi2D::Filter filter,
                   Tegra::Engines::Fermi2D::Operation operation);

    void BlitDepthStencil(const Framebuffer* dst_framebuffer, VkImageView src_depth_view,
                          VkImageView src_stencil_view, const Region2D& dst_region,
                          const Region2D& src_region, Tegra::Engines::Fermi2D::Filter filter,
                          Tegra::Engines::Fermi2D::Operation operation);

    void ConvertD32ToR32(const Framebuffer* dst_framebuffer, const ImageView& src_image_view);

    void ConvertR32ToD32(const Framebuffer* dst_framebuffer, const ImageView& src_image_view);

    void ConvertD16ToR16(const Framebuffer* dst_framebuffer, const ImageView& src_image_view);

    void ConvertR16ToD16(const Framebuffer* dst_framebuffer, const ImageView& src_image_view);

private:
    void Convert(VkPipeline pipeline, const Framebuffer* dst_framebuffer,
                 const ImageView& src_image_view);

    [[nodiscard]] VkPipeline FindOrEmplacePipeline(const BlitImagePipelineKey& key);

    [[nodiscard]] VkPipeline BlitDepthStencilPipeline(VkRenderPass renderpass);

    void ConvertDepthToColorPipeline(vk::Pipeline& pipeline, VkRenderPass renderpass);

    void ConvertColorToDepthPipeline(vk::Pipeline& pipeline, VkRenderPass renderpass);

    const Device& device;
    VKScheduler& scheduler;
    StateTracker& state_tracker;

    vk::DescriptorSetLayout one_texture_set_layout;
    vk::DescriptorSetLayout two_textures_set_layout;
    DescriptorAllocator one_texture_descriptor_allocator;
    DescriptorAllocator two_textures_descriptor_allocator;
    vk::PipelineLayout one_texture_pipeline_layout;
    vk::PipelineLayout two_textures_pipeline_layout;
    vk::ShaderModule full_screen_vert;
    vk::ShaderModule blit_color_to_color_frag;
    vk::ShaderModule blit_depth_stencil_frag;
    vk::ShaderModule convert_depth_to_float_frag;
    vk::ShaderModule convert_float_to_depth_frag;
    vk::Sampler linear_sampler;
    vk::Sampler nearest_sampler;

    std::vector<BlitImagePipelineKey> blit_color_keys;
    std::vector<vk::Pipeline> blit_color_pipelines;
    vk::Pipeline blit_depth_stencil_pipeline;
    vk::Pipeline convert_d32_to_r32_pipeline;
    vk::Pipeline convert_r32_to_d32_pipeline;
    vk::Pipeline convert_d16_to_r16_pipeline;
    vk::Pipeline convert_r16_to_d16_pipeline;
};

} // namespace Vulkan
