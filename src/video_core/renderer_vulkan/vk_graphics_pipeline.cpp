// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <span>

#include <boost/container/small_vector.hpp>
#include <boost/container/static_vector.hpp>

#include "common/bit_field.h"
#include "video_core/renderer_vulkan/maxwell_to_vk.h"
#include "video_core/renderer_vulkan/pipeline_helper.h"
#include "video_core/renderer_vulkan/vk_buffer_cache.h"
#include "video_core/renderer_vulkan/vk_graphics_pipeline.h"
#include "video_core/renderer_vulkan/vk_render_pass_cache.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_texture_cache.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/vulkan_common/vulkan_device.h"

namespace Vulkan {
namespace {
using boost::container::small_vector;
using boost::container::static_vector;
using VideoCore::Surface::PixelFormat;
using VideoCore::Surface::PixelFormatFromDepthFormat;
using VideoCore::Surface::PixelFormatFromRenderTargetFormat;

DescriptorLayoutBuilder MakeBuilder(const Device& device, std::span<const Shader::Info> infos) {
    DescriptorLayoutBuilder builder{device.GetLogical()};
    for (size_t index = 0; index < infos.size(); ++index) {
        static constexpr std::array stages{
            VK_SHADER_STAGE_VERTEX_BIT,
            VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
            VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
            VK_SHADER_STAGE_GEOMETRY_BIT,
            VK_SHADER_STAGE_FRAGMENT_BIT,
        };
        builder.Add(infos[index], stages.at(index));
    }
    return builder;
}

template <class StencilFace>
VkStencilOpState GetStencilFaceState(const StencilFace& face) {
    return {
        .failOp = MaxwellToVK::StencilOp(face.ActionStencilFail()),
        .passOp = MaxwellToVK::StencilOp(face.ActionDepthPass()),
        .depthFailOp = MaxwellToVK::StencilOp(face.ActionDepthFail()),
        .compareOp = MaxwellToVK::ComparisonOp(face.TestFunc()),
        .compareMask = 0,
        .writeMask = 0,
        .reference = 0,
    };
}

bool SupportsPrimitiveRestart(VkPrimitiveTopology topology) {
    static constexpr std::array unsupported_topologies{
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,
        VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
        // VK_PRIMITIVE_TOPOLOGY_QUAD_LIST_EXT,
    };
    return std::ranges::find(unsupported_topologies, topology) == unsupported_topologies.end();
}

VkViewportSwizzleNV UnpackViewportSwizzle(u16 swizzle) {
    union Swizzle {
        u32 raw;
        BitField<0, 3, Maxwell::ViewportSwizzle> x;
        BitField<4, 3, Maxwell::ViewportSwizzle> y;
        BitField<8, 3, Maxwell::ViewportSwizzle> z;
        BitField<12, 3, Maxwell::ViewportSwizzle> w;
    };
    const Swizzle unpacked{swizzle};
    return VkViewportSwizzleNV{
        .x = MaxwellToVK::ViewportSwizzle(unpacked.x),
        .y = MaxwellToVK::ViewportSwizzle(unpacked.y),
        .z = MaxwellToVK::ViewportSwizzle(unpacked.z),
        .w = MaxwellToVK::ViewportSwizzle(unpacked.w),
    };
}

PixelFormat DecodeFormat(u8 encoded_format) {
    const auto format{static_cast<Tegra::RenderTargetFormat>(encoded_format)};
    if (format == Tegra::RenderTargetFormat::NONE) {
        return PixelFormat::Invalid;
    }
    return PixelFormatFromRenderTargetFormat(format);
}

RenderPassKey MakeRenderPassKey(const FixedPipelineState& state) {
    RenderPassKey key;
    std::ranges::transform(state.color_formats, key.color_formats.begin(), DecodeFormat);
    if (state.depth_enabled != 0) {
        const auto depth_format{static_cast<Tegra::DepthFormat>(state.depth_format.Value())};
        key.depth_format = PixelFormatFromDepthFormat(depth_format);
    } else {
        key.depth_format = PixelFormat::Invalid;
    }
    key.samples = MaxwellToVK::MsaaMode(state.msaa_mode);
    return key;
}
} // Anonymous namespace

GraphicsPipeline::GraphicsPipeline(Tegra::Engines::Maxwell3D& maxwell3d_,
                                   Tegra::MemoryManager& gpu_memory_, VKScheduler& scheduler_,
                                   BufferCache& buffer_cache_, TextureCache& texture_cache_,
                                   const Device& device, VKDescriptorPool& descriptor_pool,
                                   VKUpdateDescriptorQueue& update_descriptor_queue_,
                                   Common::ThreadWorker* worker_thread,
                                   RenderPassCache& render_pass_cache,
                                   const FixedPipelineState& state_,
                                   std::array<vk::ShaderModule, NUM_STAGES> stages,
                                   const std::array<const Shader::Info*, NUM_STAGES>& infos)
    : maxwell3d{maxwell3d_}, gpu_memory{gpu_memory_}, texture_cache{texture_cache_},
      buffer_cache{buffer_cache_}, scheduler{scheduler_},
      update_descriptor_queue{update_descriptor_queue_}, state{state_}, spv_modules{
                                                                            std::move(stages)} {
    std::ranges::transform(infos, stage_infos.begin(),
                           [](const Shader::Info* info) { return info ? *info : Shader::Info{}; });

    DescriptorLayoutBuilder builder{MakeBuilder(device, stage_infos)};
    descriptor_set_layout = builder.CreateDescriptorSetLayout();
    descriptor_allocator = DescriptorAllocator(descriptor_pool, *descriptor_set_layout);

    auto func{[this, &device, &render_pass_cache, builder] {
        const VkDescriptorSetLayout set_layout{*descriptor_set_layout};
        pipeline_layout = builder.CreatePipelineLayout(set_layout);
        descriptor_update_template = builder.CreateTemplate(set_layout, *pipeline_layout);

        const VkRenderPass render_pass{render_pass_cache.Get(MakeRenderPassKey(state))};
        MakePipeline(device, render_pass);

        std::lock_guard lock{build_mutex};
        is_built = true;
        build_condvar.notify_one();
    }};
    if (worker_thread) {
        worker_thread->QueueWork(std::move(func));
    } else {
        func();
    }
}

void GraphicsPipeline::Configure(bool is_indexed) {
    static constexpr size_t max_images_elements = 64;
    std::array<ImageId, max_images_elements> image_view_ids;
    static_vector<u32, max_images_elements> image_view_indices;
    static_vector<VkSampler, max_images_elements> samplers;

    texture_cache.SynchronizeGraphicsDescriptors();

    const auto& regs{maxwell3d.regs};
    const bool via_header_index{regs.sampler_index == Maxwell::SamplerIndex::ViaHeaderIndex};
    for (size_t stage = 0; stage < Maxwell::MaxShaderStage; ++stage) {
        const Shader::Info& info{stage_infos[stage]};
        buffer_cache.SetEnabledUniformBuffers(stage, info.constant_buffer_mask);
        buffer_cache.UnbindGraphicsStorageBuffers(stage);
        size_t index{};
        for (const auto& desc : info.storage_buffers_descriptors) {
            ASSERT(desc.count == 1);
            buffer_cache.BindGraphicsStorageBuffer(stage, index, desc.cbuf_index, desc.cbuf_offset,
                                                   desc.is_written);
            ++index;
        }
        const auto& cbufs{maxwell3d.state.shader_stages[stage].const_buffers};
        const auto read_handle{[&](u32 cbuf_index, u32 cbuf_offset) {
            ASSERT(cbufs[cbuf_index].enabled);
            const GPUVAddr addr{cbufs[cbuf_index].address + cbuf_offset};
            const u32 raw_handle{gpu_memory.Read<u32>(addr)};
            return TextureHandle(raw_handle, via_header_index);
        }};
        const auto add_image{[&](const auto& desc) {
            const TextureHandle handle{read_handle(desc.cbuf_index, desc.cbuf_offset)};
            image_view_indices.push_back(handle.image);
        }};
        std::ranges::for_each(info.texture_buffer_descriptors, add_image);
        std::ranges::for_each(info.image_buffer_descriptors, add_image);
        for (const auto& desc : info.texture_descriptors) {
            const TextureHandle handle{read_handle(desc.cbuf_index, desc.cbuf_offset)};
            image_view_indices.push_back(handle.image);

            Sampler* const sampler{texture_cache.GetGraphicsSampler(handle.sampler)};
            samplers.push_back(sampler->Handle());
        }
        std::ranges::for_each(info.image_descriptors, add_image);
    }
    const std::span indices_span(image_view_indices.data(), image_view_indices.size());
    texture_cache.FillGraphicsImageViews(indices_span, image_view_ids);

    ImageId* texture_buffer_index{image_view_ids.data()};
    for (size_t stage = 0; stage < Maxwell::MaxShaderStage; ++stage) {
        size_t index{};
        const auto add_buffer{[&](const auto& desc) {
            ASSERT(desc.count == 1);
            bool is_written{false};
            if constexpr (std::is_same_v<decltype(desc), const Shader::ImageBufferDescriptor&>) {
                is_written = desc.is_written;
            }
            ImageView& image_view{texture_cache.GetImageView(*texture_buffer_index)};
            buffer_cache.BindGraphicsTextureBuffer(stage, index, image_view.GpuAddr(),
                                                   image_view.BufferSize(), image_view.format,
                                                   is_written);
            ++index;
            ++texture_buffer_index;
        }};
        const Shader::Info& info{stage_infos[stage]};
        buffer_cache.UnbindGraphicsTextureBuffers(stage);
        std::ranges::for_each(info.texture_buffer_descriptors, add_buffer);
        std::ranges::for_each(info.image_buffer_descriptors, add_buffer);
        texture_buffer_index += info.texture_descriptors.size();
        texture_buffer_index += info.image_descriptors.size();
    }
    buffer_cache.UpdateGraphicsBuffers(is_indexed);

    buffer_cache.BindHostGeometryBuffers(is_indexed);

    update_descriptor_queue.Acquire();

    const VkSampler* samplers_it{samplers.data()};
    const ImageId* views_it{image_view_ids.data()};
    for (size_t stage = 0; stage < Maxwell::MaxShaderStage; ++stage) {
        buffer_cache.BindHostStageBuffers(stage);
        PushImageDescriptors(stage_infos[stage], samplers_it, views_it, texture_cache,
                             update_descriptor_queue);
    }
    texture_cache.UpdateRenderTargets(false);
    scheduler.RequestRenderpass(texture_cache.GetFramebuffer());

    if (!is_built.load(std::memory_order::relaxed)) {
        // Wait for the pipeline to be built
        scheduler.Record([this](vk::CommandBuffer) {
            std::unique_lock lock{build_mutex};
            build_condvar.wait(lock, [this] { return is_built.load(std::memory_order::relaxed); });
        });
    }
    if (scheduler.UpdateGraphicsPipeline(this)) {
        scheduler.Record([this](vk::CommandBuffer cmdbuf) {
            cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
        });
    }
    if (!descriptor_set_layout) {
        return;
    }
    const VkDescriptorSet descriptor_set{descriptor_allocator.Commit()};
    update_descriptor_queue.Send(descriptor_update_template.address(), descriptor_set);

    scheduler.Record([this, descriptor_set](vk::CommandBuffer cmdbuf) {
        cmdbuf.BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline_layout, 0,
                                  descriptor_set, nullptr);
    });
}

void GraphicsPipeline::MakePipeline(const Device& device, VkRenderPass render_pass) {
    FixedPipelineState::DynamicState dynamic{};
    if (!device.IsExtExtendedDynamicStateSupported()) {
        dynamic = state.dynamic_state;
    }
    static_vector<VkVertexInputBindingDescription, 32> vertex_bindings;
    static_vector<VkVertexInputBindingDivisorDescriptionEXT, 32> vertex_binding_divisors;
    for (size_t index = 0; index < Maxwell::NumVertexArrays; ++index) {
        const bool instanced = state.binding_divisors[index] != 0;
        const auto rate = instanced ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
        vertex_bindings.push_back({
            .binding = static_cast<u32>(index),
            .stride = dynamic.vertex_strides[index],
            .inputRate = rate,
        });
        if (instanced) {
            vertex_binding_divisors.push_back({
                .binding = static_cast<u32>(index),
                .divisor = state.binding_divisors[index],
            });
        }
    }
    static_vector<VkVertexInputAttributeDescription, 32> vertex_attributes;
    const auto& input_attributes = stage_infos[0].input_generics;
    for (size_t index = 0; index < state.attributes.size(); ++index) {
        const auto& attribute = state.attributes[index];
        if (!attribute.enabled || !input_attributes[index].used) {
            continue;
        }
        vertex_attributes.push_back({
            .location = static_cast<u32>(index),
            .binding = attribute.buffer,
            .format = MaxwellToVK::VertexFormat(attribute.Type(), attribute.Size()),
            .offset = attribute.offset,
        });
    }
    VkPipelineVertexInputStateCreateInfo vertex_input_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = static_cast<u32>(vertex_bindings.size()),
        .pVertexBindingDescriptions = vertex_bindings.data(),
        .vertexAttributeDescriptionCount = static_cast<u32>(vertex_attributes.size()),
        .pVertexAttributeDescriptions = vertex_attributes.data(),
    };
    const VkPipelineVertexInputDivisorStateCreateInfoEXT input_divisor_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT,
        .pNext = nullptr,
        .vertexBindingDivisorCount = static_cast<u32>(vertex_binding_divisors.size()),
        .pVertexBindingDivisors = vertex_binding_divisors.data(),
    };
    if (!vertex_binding_divisors.empty()) {
        vertex_input_ci.pNext = &input_divisor_ci;
    }
    const auto input_assembly_topology = MaxwellToVK::PrimitiveTopology(device, state.topology);
    const VkPipelineInputAssemblyStateCreateInfo input_assembly_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = MaxwellToVK::PrimitiveTopology(device, state.topology),
        .primitiveRestartEnable = state.primitive_restart_enable != 0 &&
                                  SupportsPrimitiveRestart(input_assembly_topology),
    };
    const VkPipelineTessellationStateCreateInfo tessellation_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .patchControlPoints = state.patch_control_points_minus_one.Value() + 1,
    };
    VkPipelineViewportStateCreateInfo viewport_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = Maxwell::NumViewports,
        .pViewports = nullptr,
        .scissorCount = Maxwell::NumViewports,
        .pScissors = nullptr,
    };
    std::array<VkViewportSwizzleNV, Maxwell::NumViewports> swizzles;
    std::ranges::transform(state.viewport_swizzles, swizzles.begin(), UnpackViewportSwizzle);
    VkPipelineViewportSwizzleStateCreateInfoNV swizzle_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_SWIZZLE_STATE_CREATE_INFO_NV,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = Maxwell::NumViewports,
        .pViewportSwizzles = swizzles.data(),
    };
    if (device.IsNvViewportSwizzleSupported()) {
        viewport_ci.pNext = &swizzle_ci;
    }

    const VkPipelineRasterizationStateCreateInfo rasterization_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthClampEnable =
            static_cast<VkBool32>(state.depth_clamp_disabled == 0 ? VK_TRUE : VK_FALSE),
        .rasterizerDiscardEnable =
            static_cast<VkBool32>(state.rasterize_enable == 0 ? VK_TRUE : VK_FALSE),
        .polygonMode =
            MaxwellToVK::PolygonMode(FixedPipelineState::UnpackPolygonMode(state.polygon_mode)),
        .cullMode = static_cast<VkCullModeFlags>(
            dynamic.cull_enable ? MaxwellToVK::CullFace(dynamic.CullFace()) : VK_CULL_MODE_NONE),
        .frontFace = MaxwellToVK::FrontFace(dynamic.FrontFace()),
        .depthBiasEnable = state.depth_bias_enable,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
        .lineWidth = 1.0f,
    };
    const VkPipelineMultisampleStateCreateInfo multisample_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .rasterizationSamples = MaxwellToVK::MsaaMode(state.msaa_mode),
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 0.0f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };
    const VkPipelineDepthStencilStateCreateInfo depth_stencil_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthTestEnable = dynamic.depth_test_enable,
        .depthWriteEnable = dynamic.depth_write_enable,
        .depthCompareOp = dynamic.depth_test_enable
                              ? MaxwellToVK::ComparisonOp(dynamic.DepthTestFunc())
                              : VK_COMPARE_OP_ALWAYS,
        .depthBoundsTestEnable = dynamic.depth_bounds_enable,
        .stencilTestEnable = dynamic.stencil_enable,
        .front = GetStencilFaceState(dynamic.front),
        .back = GetStencilFaceState(dynamic.back),
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 0.0f,
    };
    static_vector<VkPipelineColorBlendAttachmentState, Maxwell::NumRenderTargets> cb_attachments;
    for (size_t index = 0; index < Maxwell::NumRenderTargets; ++index) {
        static constexpr std::array mask_table{
            VK_COLOR_COMPONENT_R_BIT,
            VK_COLOR_COMPONENT_G_BIT,
            VK_COLOR_COMPONENT_B_BIT,
            VK_COLOR_COMPONENT_A_BIT,
        };
        const auto format{static_cast<Tegra::RenderTargetFormat>(state.color_formats[index])};
        if (format == Tegra::RenderTargetFormat::NONE) {
            continue;
        }
        const auto& blend{state.attachments[index]};
        const std::array mask{blend.Mask()};
        VkColorComponentFlags write_mask{};
        for (size_t i = 0; i < mask_table.size(); ++i) {
            write_mask |= mask[i] ? mask_table[i] : 0;
        }
        cb_attachments.push_back({
            .blendEnable = blend.enable != 0,
            .srcColorBlendFactor = MaxwellToVK::BlendFactor(blend.SourceRGBFactor()),
            .dstColorBlendFactor = MaxwellToVK::BlendFactor(blend.DestRGBFactor()),
            .colorBlendOp = MaxwellToVK::BlendEquation(blend.EquationRGB()),
            .srcAlphaBlendFactor = MaxwellToVK::BlendFactor(blend.SourceAlphaFactor()),
            .dstAlphaBlendFactor = MaxwellToVK::BlendFactor(blend.DestAlphaFactor()),
            .alphaBlendOp = MaxwellToVK::BlendEquation(blend.EquationAlpha()),
            .colorWriteMask = write_mask,
        });
    }
    const VkPipelineColorBlendStateCreateInfo color_blend_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = static_cast<u32>(cb_attachments.size()),
        .pAttachments = cb_attachments.data(),
        .blendConstants = {},
    };
    static_vector<VkDynamicState, 17> dynamic_states{
        VK_DYNAMIC_STATE_VIEWPORT,           VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_DEPTH_BIAS,         VK_DYNAMIC_STATE_BLEND_CONSTANTS,
        VK_DYNAMIC_STATE_DEPTH_BOUNDS,       VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
        VK_DYNAMIC_STATE_STENCIL_WRITE_MASK, VK_DYNAMIC_STATE_STENCIL_REFERENCE,
    };
    if (device.IsExtExtendedDynamicStateSupported()) {
        static constexpr std::array extended{
            VK_DYNAMIC_STATE_CULL_MODE_EXT,
            VK_DYNAMIC_STATE_FRONT_FACE_EXT,
            VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT,
            VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT,
            VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT,
            VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT,
            VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT,
            VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT,
            VK_DYNAMIC_STATE_STENCIL_OP_EXT,
        };
        dynamic_states.insert(dynamic_states.end(), extended.begin(), extended.end());
    }
    const VkPipelineDynamicStateCreateInfo dynamic_state_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .dynamicStateCount = static_cast<u32>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data(),
    };
    [[maybe_unused]] const VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT subgroup_size_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT,
        .pNext = nullptr,
        .requiredSubgroupSize = GuestWarpSize,
    };
    static_vector<VkPipelineShaderStageCreateInfo, 5> shader_stages;
    for (size_t stage = 0; stage < Maxwell::MaxShaderStage; ++stage) {
        if (!spv_modules[stage]) {
            continue;
        }
        [[maybe_unused]] auto& stage_ci =
            shader_stages.emplace_back(VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = MaxwellToVK::ShaderStage(static_cast<Tegra::Engines::ShaderType>(stage)),
                .module = *spv_modules[stage],
                .pName = "main",
                .pSpecializationInfo = nullptr,
            });
        /*
        if (program[stage]->entries.uses_warps && device.IsGuestWarpSizeSupported(stage_ci.stage)) {
            stage_ci.pNext = &subgroup_size_ci;
        }
        */
    }
    pipeline = device.GetLogical().CreateGraphicsPipeline({
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = static_cast<u32>(shader_stages.size()),
        .pStages = shader_stages.data(),
        .pVertexInputState = &vertex_input_ci,
        .pInputAssemblyState = &input_assembly_ci,
        .pTessellationState = &tessellation_ci,
        .pViewportState = &viewport_ci,
        .pRasterizationState = &rasterization_ci,
        .pMultisampleState = &multisample_ci,
        .pDepthStencilState = &depth_stencil_ci,
        .pColorBlendState = &color_blend_ci,
        .pDynamicState = &dynamic_state_ci,
        .layout = *pipeline_layout,
        .renderPass = render_pass,
        .subpass = 0,
        .basePipelineHandle = nullptr,
        .basePipelineIndex = 0,
    });
}

} // namespace Vulkan
