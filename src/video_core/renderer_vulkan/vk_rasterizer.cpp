// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <memory>
#include <mutex>
#include <vector>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "core/core.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_vulkan/blit_image.h"
#include "video_core/renderer_vulkan/fixed_pipeline_state.h"
#include "video_core/renderer_vulkan/maxwell_to_vk.h"
#include "video_core/renderer_vulkan/renderer_vulkan.h"
#include "video_core/renderer_vulkan/vk_buffer_cache.h"
#include "video_core/renderer_vulkan/vk_compute_pipeline.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_staging_buffer_pool.h"
#include "video_core/renderer_vulkan/vk_state_tracker.h"
#include "video_core/renderer_vulkan/vk_texture_cache.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/shader_cache.h"
#include "video_core/texture_cache/texture_cache.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

using Maxwell = Tegra::Engines::Maxwell3D::Regs;
using VideoCommon::ImageViewId;
using VideoCommon::ImageViewType;

MICROPROFILE_DEFINE(Vulkan_WaitForWorker, "Vulkan", "Wait for worker", MP_RGB(255, 192, 192));
MICROPROFILE_DEFINE(Vulkan_Drawing, "Vulkan", "Record drawing", MP_RGB(192, 128, 128));
MICROPROFILE_DEFINE(Vulkan_Compute, "Vulkan", "Record compute", MP_RGB(192, 128, 128));
MICROPROFILE_DEFINE(Vulkan_Clearing, "Vulkan", "Record clearing", MP_RGB(192, 128, 128));
MICROPROFILE_DEFINE(Vulkan_PipelineCache, "Vulkan", "Pipeline cache", MP_RGB(192, 128, 128));

namespace {
struct DrawParams {
    u32 base_instance;
    u32 num_instances;
    u32 base_vertex;
    u32 num_vertices;
    bool is_indexed;
};

constexpr auto COMPUTE_SHADER_INDEX = static_cast<size_t>(Tegra::Engines::ShaderType::Compute);

VkViewport GetViewportState(const Device& device, const Maxwell& regs, size_t index) {
    const auto& src = regs.viewport_transform[index];
    const float width = src.scale_x * 2.0f;
    const float height = src.scale_y * 2.0f;
    const float reduce_z = regs.depth_mode == Maxwell::DepthMode::MinusOneToOne ? 1.0f : 0.0f;
    VkViewport viewport{
        .x = src.translate_x - src.scale_x,
        .y = src.translate_y - src.scale_y,
        .width = width != 0.0f ? width : 1.0f,
        .height = height != 0.0f ? height : 1.0f,
        .minDepth = src.translate_z - src.scale_z * reduce_z,
        .maxDepth = src.translate_z + src.scale_z,
    };
    if (!device.IsExtDepthRangeUnrestrictedSupported()) {
        viewport.minDepth = std::clamp(viewport.minDepth, 0.0f, 1.0f);
        viewport.maxDepth = std::clamp(viewport.maxDepth, 0.0f, 1.0f);
    }
    return viewport;
}

VkRect2D GetScissorState(const Maxwell& regs, size_t index) {
    const auto& src = regs.scissor_test[index];
    VkRect2D scissor;
    if (src.enable) {
        scissor.offset.x = static_cast<s32>(src.min_x);
        scissor.offset.y = static_cast<s32>(src.min_y);
        scissor.extent.width = src.max_x - src.min_x;
        scissor.extent.height = src.max_y - src.min_y;
    } else {
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = std::numeric_limits<s32>::max();
        scissor.extent.height = std::numeric_limits<s32>::max();
    }
    return scissor;
}

struct TextureHandle {
    constexpr TextureHandle(u32 data, bool via_header_index) {
        const Tegra::Texture::TextureHandle handle{data};
        image = handle.tic_id;
        sampler = via_header_index ? image : handle.tsc_id.Value();
    }

    u32 image;
    u32 sampler;
};

DrawParams MakeDrawParams(const Maxwell& regs, u32 num_instances, bool is_instanced,
                          bool is_indexed) {
    DrawParams params{
        .base_instance = regs.vb_base_instance,
        .num_instances = is_instanced ? num_instances : 1,
        .base_vertex = is_indexed ? regs.vb_element_base : regs.vertex_buffer.first,
        .num_vertices = is_indexed ? regs.index_array.count : regs.vertex_buffer.count,
        .is_indexed = is_indexed,
    };
    if (regs.draw.topology == Maxwell::PrimitiveTopology::Quads) {
        // 6 triangle vertices per quad, base vertex is part of the index
        // See BindQuadArrayIndexBuffer for more details
        params.num_vertices = (params.num_vertices / 4) * 6;
        params.base_vertex = 0;
        params.is_indexed = true;
    }
    return params;
}
} // Anonymous namespace

RasterizerVulkan::RasterizerVulkan(Core::Frontend::EmuWindow& emu_window_, Tegra::GPU& gpu_,
                                   Tegra::MemoryManager& gpu_memory_,
                                   Core::Memory::Memory& cpu_memory_, VKScreenInfo& screen_info_,
                                   const Device& device_, MemoryAllocator& memory_allocator_,
                                   StateTracker& state_tracker_, VKScheduler& scheduler_)
    : RasterizerAccelerated{cpu_memory_}, gpu{gpu_},
      gpu_memory{gpu_memory_}, maxwell3d{gpu.Maxwell3D()}, kepler_compute{gpu.KeplerCompute()},
      screen_info{screen_info_}, device{device_}, memory_allocator{memory_allocator_},
      state_tracker{state_tracker_}, scheduler{scheduler_},
      staging_pool(device, memory_allocator, scheduler), descriptor_pool(device, scheduler),
      update_descriptor_queue(device, scheduler),
      blit_image(device, scheduler, state_tracker, descriptor_pool),
      astc_decoder_pass(device, scheduler, descriptor_pool, staging_pool, update_descriptor_queue,
                        memory_allocator),
      render_pass_cache(device), texture_cache_runtime{device,           scheduler,
                                                       memory_allocator, staging_pool,
                                                       blit_image,       astc_decoder_pass,
                                                       render_pass_cache},
      texture_cache(texture_cache_runtime, *this, maxwell3d, kepler_compute, gpu_memory),
      buffer_cache_runtime(device, memory_allocator, scheduler, staging_pool,
                           update_descriptor_queue, descriptor_pool),
      buffer_cache(*this, maxwell3d, kepler_compute, gpu_memory, cpu_memory_, buffer_cache_runtime),
      pipeline_cache(*this, maxwell3d, kepler_compute, gpu_memory, device, scheduler,
                     descriptor_pool, update_descriptor_queue, render_pass_cache, buffer_cache,
                     texture_cache),
      query_cache{*this, maxwell3d, gpu_memory, device, scheduler}, accelerate_dma{ buffer_cache },
      fence_manager(*this, gpu, texture_cache, buffer_cache, query_cache, device, scheduler),
      wfi_event(device.GetLogical().CreateEvent()) {
    scheduler.SetQueryCache(query_cache);
}

RasterizerVulkan::~RasterizerVulkan() = default;

void RasterizerVulkan::Draw(bool is_indexed, bool is_instanced) {
    MICROPROFILE_SCOPE(Vulkan_Drawing);

    SCOPE_EXIT({ gpu.TickWork(); });
    FlushWork();

    query_cache.UpdateCounters();

    GraphicsPipeline* const pipeline{pipeline_cache.CurrentGraphicsPipeline()};
    if (!pipeline) {
        return;
    }
    std::scoped_lock lock{buffer_cache.mutex, texture_cache.mutex};
    pipeline->Configure(is_indexed);

    BeginTransformFeedback();

    UpdateDynamicStates();

    const auto& regs{maxwell3d.regs};
    const u32 num_instances{maxwell3d.mme_draw.instance_count};
    const DrawParams draw_params{MakeDrawParams(regs, num_instances, is_instanced, is_indexed)};
    scheduler.Record([draw_params](vk::CommandBuffer cmdbuf) {
        if (draw_params.is_indexed) {
            cmdbuf.DrawIndexed(draw_params.num_vertices, draw_params.num_instances, 0,
                               draw_params.base_vertex, draw_params.base_instance);
        } else {
            cmdbuf.Draw(draw_params.num_vertices, draw_params.num_instances,
                        draw_params.base_vertex, draw_params.base_instance);
        }
    });
    EndTransformFeedback();
}

void RasterizerVulkan::Clear() {
    MICROPROFILE_SCOPE(Vulkan_Clearing);

    if (!maxwell3d.ShouldExecute()) {
        return;
    }

    query_cache.UpdateCounters();

    const auto& regs = maxwell3d.regs;
    const bool use_color = regs.clear_buffers.R || regs.clear_buffers.G || regs.clear_buffers.B ||
                           regs.clear_buffers.A;
    const bool use_depth = regs.clear_buffers.Z;
    const bool use_stencil = regs.clear_buffers.S;
    if (!use_color && !use_depth && !use_stencil) {
        return;
    }

    std::scoped_lock lock{texture_cache.mutex};
    texture_cache.UpdateRenderTargets(true);
    const Framebuffer* const framebuffer = texture_cache.GetFramebuffer();
    const VkExtent2D render_area = framebuffer->RenderArea();
    scheduler.RequestRenderpass(framebuffer);

    VkClearRect clear_rect{
        .rect = GetScissorState(regs, 0),
        .baseArrayLayer = regs.clear_buffers.layer,
        .layerCount = 1,
    };
    if (clear_rect.rect.extent.width == 0 || clear_rect.rect.extent.height == 0) {
        return;
    }
    clear_rect.rect.extent = VkExtent2D{
        .width = std::min(clear_rect.rect.extent.width, render_area.width),
        .height = std::min(clear_rect.rect.extent.height, render_area.height),
    };

    const u32 color_attachment = regs.clear_buffers.RT;
    const auto attachment_aspect_mask = framebuffer->ImageRanges()[color_attachment].aspectMask;
    const bool is_color_rt = (attachment_aspect_mask & VK_IMAGE_ASPECT_COLOR_BIT) != 0;
    if (use_color && is_color_rt) {
        VkClearValue clear_value;
        std::memcpy(clear_value.color.float32, regs.clear_color, sizeof(regs.clear_color));

        scheduler.Record([color_attachment, clear_value, clear_rect](vk::CommandBuffer cmdbuf) {
            const VkClearAttachment attachment{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .colorAttachment = color_attachment,
                .clearValue = clear_value,
            };
            cmdbuf.ClearAttachments(attachment, clear_rect);
        });
    }

    if (!use_depth && !use_stencil) {
        return;
    }
    VkImageAspectFlags aspect_flags = 0;
    if (use_depth) {
        aspect_flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    if (use_stencil) {
        aspect_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    scheduler.Record([clear_depth = regs.clear_depth, clear_stencil = regs.clear_stencil,
                      clear_rect, aspect_flags](vk::CommandBuffer cmdbuf) {
        VkClearAttachment attachment;
        attachment.aspectMask = aspect_flags;
        attachment.colorAttachment = 0;
        attachment.clearValue.depthStencil.depth = clear_depth;
        attachment.clearValue.depthStencil.stencil = clear_stencil;
        cmdbuf.ClearAttachments(attachment, clear_rect);
    });
}

void RasterizerVulkan::DispatchCompute() {
    ComputePipeline* const pipeline{pipeline_cache.CurrentComputePipeline()};
    if (!pipeline) {
        return;
    }
    std::scoped_lock lock{texture_cache.mutex, buffer_cache.mutex};
    pipeline->Configure(kepler_compute, gpu_memory, scheduler, buffer_cache, texture_cache);

    const auto& qmd{kepler_compute.launch_description};
    const std::array<u32, 3> dim{qmd.grid_dim_x, qmd.grid_dim_y, qmd.grid_dim_z};
    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([dim](vk::CommandBuffer cmdbuf) { cmdbuf.Dispatch(dim[0], dim[1], dim[2]); });
}

void RasterizerVulkan::ResetCounter(VideoCore::QueryType type) {
    query_cache.ResetCounter(type);
}

void RasterizerVulkan::Query(GPUVAddr gpu_addr, VideoCore::QueryType type,
                             std::optional<u64> timestamp) {
    query_cache.Query(gpu_addr, type, timestamp);
}

void RasterizerVulkan::BindGraphicsUniformBuffer(size_t stage, u32 index, GPUVAddr gpu_addr,
                                                 u32 size) {
    buffer_cache.BindGraphicsUniformBuffer(stage, index, gpu_addr, size);
}

void Vulkan::RasterizerVulkan::DisableGraphicsUniformBuffer(size_t stage, u32 index) {
    buffer_cache.DisableGraphicsUniformBuffer(stage, index);
}

void RasterizerVulkan::FlushAll() {}

void RasterizerVulkan::FlushRegion(VAddr addr, u64 size) {
    if (addr == 0 || size == 0) {
        return;
    }
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.DownloadMemory(addr, size);
    }
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.DownloadMemory(addr, size);
    }
    query_cache.FlushRegion(addr, size);
}

bool RasterizerVulkan::MustFlushRegion(VAddr addr, u64 size) {
    std::scoped_lock lock{texture_cache.mutex, buffer_cache.mutex};
    if (!Settings::IsGPULevelHigh()) {
        return buffer_cache.IsRegionGpuModified(addr, size);
    }
    return texture_cache.IsRegionGpuModified(addr, size) ||
           buffer_cache.IsRegionGpuModified(addr, size);
}

void RasterizerVulkan::InvalidateRegion(VAddr addr, u64 size) {
    if (addr == 0 || size == 0) {
        return;
    }
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.WriteMemory(addr, size);
    }
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.WriteMemory(addr, size);
    }
    pipeline_cache.InvalidateRegion(addr, size);
    query_cache.InvalidateRegion(addr, size);
}

void RasterizerVulkan::OnCPUWrite(VAddr addr, u64 size) {
    if (addr == 0 || size == 0) {
        return;
    }
    pipeline_cache.OnCPUWrite(addr, size);
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.WriteMemory(addr, size);
    }
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.CachedWriteMemory(addr, size);
    }
}

void RasterizerVulkan::SyncGuestHost() {
    pipeline_cache.SyncGuestHost();
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.FlushCachedWrites();
    }
}

void RasterizerVulkan::UnmapMemory(VAddr addr, u64 size) {
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.UnmapMemory(addr, size);
    }
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.WriteMemory(addr, size);
    }
    pipeline_cache.OnCPUWrite(addr, size);
}

void RasterizerVulkan::ModifyGPUMemory(GPUVAddr addr, u64 size) {
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.UnmapGPUMemory(addr, size);
    }
}

void RasterizerVulkan::SignalSemaphore(GPUVAddr addr, u32 value) {
    if (!gpu.IsAsync()) {
        gpu_memory.Write<u32>(addr, value);
        return;
    }
    fence_manager.SignalSemaphore(addr, value);
}

void RasterizerVulkan::SignalSyncPoint(u32 value) {
    if (!gpu.IsAsync()) {
        gpu.IncrementSyncPoint(value);
        return;
    }
    fence_manager.SignalSyncPoint(value);
}

void RasterizerVulkan::SignalReference() {
    if (!gpu.IsAsync()) {
        return;
    }
    fence_manager.SignalOrdering();
}

void RasterizerVulkan::ReleaseFences() {
    if (!gpu.IsAsync()) {
        return;
    }
    fence_manager.WaitPendingFences();
}

void RasterizerVulkan::FlushAndInvalidateRegion(VAddr addr, u64 size) {
    if (Settings::IsGPULevelExtreme()) {
        FlushRegion(addr, size);
    }
    InvalidateRegion(addr, size);
}

void RasterizerVulkan::WaitForIdle() {
    // Everything but wait pixel operations. This intentionally includes FRAGMENT_SHADER_BIT because
    // fragment shaders can still write storage buffers.
    VkPipelineStageFlags flags =
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
        VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
        VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
    if (device.IsExtTransformFeedbackSupported()) {
        flags |= VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT;
    }

    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([event = *wfi_event, flags](vk::CommandBuffer cmdbuf) {
        cmdbuf.SetEvent(event, flags);
        cmdbuf.WaitEvents(event, flags, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, {}, {}, {});
    });
    SignalReference();
}

void RasterizerVulkan::FragmentBarrier() {
    // We already put barriers when a render pass finishes
}

void RasterizerVulkan::TiledCacheBarrier() {
    // TODO: Implementing tiled barriers requires rewriting a good chunk of the Vulkan backend
}

void RasterizerVulkan::FlushCommands() {
    if (draw_counter == 0) {
        return;
    }
    draw_counter = 0;
    scheduler.Flush();
}

void RasterizerVulkan::TickFrame() {
    draw_counter = 0;
    update_descriptor_queue.TickFrame();
    fence_manager.TickFrame();
    staging_pool.TickFrame();
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.TickFrame();
    }
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.TickFrame();
    }
}

bool RasterizerVulkan::AccelerateSurfaceCopy(const Tegra::Engines::Fermi2D::Surface& src,
                                             const Tegra::Engines::Fermi2D::Surface& dst,
                                             const Tegra::Engines::Fermi2D::Config& copy_config) {
    std::scoped_lock lock{texture_cache.mutex};
    texture_cache.BlitImage(dst, src, copy_config);
    return true;
}

Tegra::Engines::AccelerateDMAInterface& RasterizerVulkan::AccessAccelerateDMA() {
    return accelerate_dma;
}

bool RasterizerVulkan::AccelerateDisplay(const Tegra::FramebufferConfig& config,
                                         VAddr framebuffer_addr, u32 pixel_stride) {
    if (!framebuffer_addr) {
        return false;
    }
    std::scoped_lock lock{texture_cache.mutex};
    ImageView* const image_view = texture_cache.TryFindFramebufferImageView(framebuffer_addr);
    if (!image_view) {
        return false;
    }
    screen_info.image_view = image_view->Handle(Shader::TextureType::Color2D);
    screen_info.width = image_view->size.width;
    screen_info.height = image_view->size.height;
    screen_info.is_srgb = VideoCore::Surface::IsPixelFormatSRGB(image_view->format);
    return true;
}

void RasterizerVulkan::LoadDiskResources(u64 title_id, std::stop_token stop_loading,
                                         const VideoCore::DiskResourceLoadCallback& callback) {
    pipeline_cache.LoadDiskResources(title_id, stop_loading, callback);
}

void RasterizerVulkan::FlushWork() {
    static constexpr u32 DRAWS_TO_DISPATCH = 4096;

    // Only check multiples of 8 draws
    static_assert(DRAWS_TO_DISPATCH % 8 == 0);
    if ((++draw_counter & 7) != 7) {
        return;
    }
    if (draw_counter < DRAWS_TO_DISPATCH) {
        // Send recorded tasks to the worker thread
        scheduler.DispatchWork();
        return;
    }
    // Otherwise (every certain number of draws) flush execution.
    // This submits commands to the Vulkan driver.
    scheduler.Flush();
    draw_counter = 0;
}

AccelerateDMA::AccelerateDMA(BufferCache& buffer_cache_) : buffer_cache{buffer_cache_} {}

bool AccelerateDMA::BufferCopy(GPUVAddr src_address, GPUVAddr dest_address, u64 amount) {
    std::scoped_lock lock{buffer_cache.mutex};
    return buffer_cache.DMACopy(src_address, dest_address, amount);
}

void RasterizerVulkan::UpdateDynamicStates() {
    auto& regs = maxwell3d.regs;
    UpdateViewportsState(regs);
    UpdateScissorsState(regs);
    UpdateDepthBias(regs);
    UpdateBlendConstants(regs);
    UpdateDepthBounds(regs);
    UpdateStencilFaces(regs);
    if (device.IsExtExtendedDynamicStateSupported()) {
        UpdateCullMode(regs);
        UpdateDepthBoundsTestEnable(regs);
        UpdateDepthTestEnable(regs);
        UpdateDepthWriteEnable(regs);
        UpdateDepthCompareOp(regs);
        UpdateFrontFace(regs);
        UpdateStencilOp(regs);
        UpdateStencilTestEnable(regs);
    }
}

void RasterizerVulkan::BeginTransformFeedback() {
    const auto& regs = maxwell3d.regs;
    if (regs.tfb_enabled == 0) {
        return;
    }
    if (!device.IsExtTransformFeedbackSupported()) {
        LOG_ERROR(Render_Vulkan, "Transform feedbacks used but not supported");
        return;
    }
    UNIMPLEMENTED_IF(regs.IsShaderConfigEnabled(Maxwell::ShaderProgram::TesselationControl) ||
                     regs.IsShaderConfigEnabled(Maxwell::ShaderProgram::TesselationEval) ||
                     regs.IsShaderConfigEnabled(Maxwell::ShaderProgram::Geometry));
    scheduler.Record(
        [](vk::CommandBuffer cmdbuf) { cmdbuf.BeginTransformFeedbackEXT(0, 0, nullptr, nullptr); });
}

void RasterizerVulkan::EndTransformFeedback() {
    const auto& regs = maxwell3d.regs;
    if (regs.tfb_enabled == 0) {
        return;
    }
    if (!device.IsExtTransformFeedbackSupported()) {
        return;
    }
    scheduler.Record(
        [](vk::CommandBuffer cmdbuf) { cmdbuf.EndTransformFeedbackEXT(0, 0, nullptr, nullptr); });
}

void RasterizerVulkan::UpdateViewportsState(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchViewports()) {
        return;
    }
    const std::array viewports{
        GetViewportState(device, regs, 0),  GetViewportState(device, regs, 1),
        GetViewportState(device, regs, 2),  GetViewportState(device, regs, 3),
        GetViewportState(device, regs, 4),  GetViewportState(device, regs, 5),
        GetViewportState(device, regs, 6),  GetViewportState(device, regs, 7),
        GetViewportState(device, regs, 8),  GetViewportState(device, regs, 9),
        GetViewportState(device, regs, 10), GetViewportState(device, regs, 11),
        GetViewportState(device, regs, 12), GetViewportState(device, regs, 13),
        GetViewportState(device, regs, 14), GetViewportState(device, regs, 15),
    };
    scheduler.Record([viewports](vk::CommandBuffer cmdbuf) { cmdbuf.SetViewport(0, viewports); });
}

void RasterizerVulkan::UpdateScissorsState(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchScissors()) {
        return;
    }
    const std::array scissors{
        GetScissorState(regs, 0),  GetScissorState(regs, 1),  GetScissorState(regs, 2),
        GetScissorState(regs, 3),  GetScissorState(regs, 4),  GetScissorState(regs, 5),
        GetScissorState(regs, 6),  GetScissorState(regs, 7),  GetScissorState(regs, 8),
        GetScissorState(regs, 9),  GetScissorState(regs, 10), GetScissorState(regs, 11),
        GetScissorState(regs, 12), GetScissorState(regs, 13), GetScissorState(regs, 14),
        GetScissorState(regs, 15),
    };
    scheduler.Record([scissors](vk::CommandBuffer cmdbuf) { cmdbuf.SetScissor(0, scissors); });
}

void RasterizerVulkan::UpdateDepthBias(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchDepthBias()) {
        return;
    }
    scheduler.Record([constant = regs.polygon_offset_units, clamp = regs.polygon_offset_clamp,
                      factor = regs.polygon_offset_factor](vk::CommandBuffer cmdbuf) {
        cmdbuf.SetDepthBias(constant, clamp, factor / 2.0f);
    });
}

void RasterizerVulkan::UpdateBlendConstants(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchBlendConstants()) {
        return;
    }
    const std::array blend_color = {regs.blend_color.r, regs.blend_color.g, regs.blend_color.b,
                                    regs.blend_color.a};
    scheduler.Record(
        [blend_color](vk::CommandBuffer cmdbuf) { cmdbuf.SetBlendConstants(blend_color.data()); });
}

void RasterizerVulkan::UpdateDepthBounds(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchDepthBounds()) {
        return;
    }
    scheduler.Record([min = regs.depth_bounds[0], max = regs.depth_bounds[1]](
                         vk::CommandBuffer cmdbuf) { cmdbuf.SetDepthBounds(min, max); });
}

void RasterizerVulkan::UpdateStencilFaces(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchStencilProperties()) {
        return;
    }
    if (regs.stencil_two_side_enable) {
        // Separate values per face
        scheduler.Record(
            [front_ref = regs.stencil_front_func_ref, front_write_mask = regs.stencil_front_mask,
             front_test_mask = regs.stencil_front_func_mask, back_ref = regs.stencil_back_func_ref,
             back_write_mask = regs.stencil_back_mask,
             back_test_mask = regs.stencil_back_func_mask](vk::CommandBuffer cmdbuf) {
                // Front face
                cmdbuf.SetStencilReference(VK_STENCIL_FACE_FRONT_BIT, front_ref);
                cmdbuf.SetStencilWriteMask(VK_STENCIL_FACE_FRONT_BIT, front_write_mask);
                cmdbuf.SetStencilCompareMask(VK_STENCIL_FACE_FRONT_BIT, front_test_mask);

                // Back face
                cmdbuf.SetStencilReference(VK_STENCIL_FACE_BACK_BIT, back_ref);
                cmdbuf.SetStencilWriteMask(VK_STENCIL_FACE_BACK_BIT, back_write_mask);
                cmdbuf.SetStencilCompareMask(VK_STENCIL_FACE_BACK_BIT, back_test_mask);
            });
    } else {
        // Front face defines both faces
        scheduler.Record([ref = regs.stencil_back_func_ref, write_mask = regs.stencil_back_mask,
                          test_mask = regs.stencil_back_func_mask](vk::CommandBuffer cmdbuf) {
            cmdbuf.SetStencilReference(VK_STENCIL_FACE_FRONT_AND_BACK, ref);
            cmdbuf.SetStencilWriteMask(VK_STENCIL_FACE_FRONT_AND_BACK, write_mask);
            cmdbuf.SetStencilCompareMask(VK_STENCIL_FACE_FRONT_AND_BACK, test_mask);
        });
    }
}

void RasterizerVulkan::UpdateCullMode(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchCullMode()) {
        return;
    }
    scheduler.Record(
        [enabled = regs.cull_test_enabled, cull_face = regs.cull_face](vk::CommandBuffer cmdbuf) {
            cmdbuf.SetCullModeEXT(enabled ? MaxwellToVK::CullFace(cull_face) : VK_CULL_MODE_NONE);
        });
}

void RasterizerVulkan::UpdateDepthBoundsTestEnable(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchDepthBoundsTestEnable()) {
        return;
    }
    scheduler.Record([enable = regs.depth_bounds_enable](vk::CommandBuffer cmdbuf) {
        cmdbuf.SetDepthBoundsTestEnableEXT(enable);
    });
}

void RasterizerVulkan::UpdateDepthTestEnable(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchDepthTestEnable()) {
        return;
    }
    scheduler.Record([enable = regs.depth_test_enable](vk::CommandBuffer cmdbuf) {
        cmdbuf.SetDepthTestEnableEXT(enable);
    });
}

void RasterizerVulkan::UpdateDepthWriteEnable(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchDepthWriteEnable()) {
        return;
    }
    scheduler.Record([enable = regs.depth_write_enabled](vk::CommandBuffer cmdbuf) {
        cmdbuf.SetDepthWriteEnableEXT(enable);
    });
}

void RasterizerVulkan::UpdateDepthCompareOp(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchDepthCompareOp()) {
        return;
    }
    scheduler.Record([func = regs.depth_test_func](vk::CommandBuffer cmdbuf) {
        cmdbuf.SetDepthCompareOpEXT(MaxwellToVK::ComparisonOp(func));
    });
}

void RasterizerVulkan::UpdateFrontFace(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchFrontFace()) {
        return;
    }

    VkFrontFace front_face = MaxwellToVK::FrontFace(regs.front_face);
    if (regs.screen_y_control.triangle_rast_flip != 0) {
        front_face = front_face == VK_FRONT_FACE_CLOCKWISE ? VK_FRONT_FACE_COUNTER_CLOCKWISE
                                                           : VK_FRONT_FACE_CLOCKWISE;
    }
    scheduler.Record(
        [front_face](vk::CommandBuffer cmdbuf) { cmdbuf.SetFrontFaceEXT(front_face); });
}

void RasterizerVulkan::UpdateStencilOp(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchStencilOp()) {
        return;
    }
    const Maxwell::StencilOp fail = regs.stencil_front_op_fail;
    const Maxwell::StencilOp zfail = regs.stencil_front_op_zfail;
    const Maxwell::StencilOp zpass = regs.stencil_front_op_zpass;
    const Maxwell::ComparisonOp compare = regs.stencil_front_func_func;
    if (regs.stencil_two_side_enable) {
        scheduler.Record([fail, zfail, zpass, compare](vk::CommandBuffer cmdbuf) {
            cmdbuf.SetStencilOpEXT(VK_STENCIL_FACE_FRONT_AND_BACK, MaxwellToVK::StencilOp(fail),
                                   MaxwellToVK::StencilOp(zpass), MaxwellToVK::StencilOp(zfail),
                                   MaxwellToVK::ComparisonOp(compare));
        });
    } else {
        const Maxwell::StencilOp back_fail = regs.stencil_back_op_fail;
        const Maxwell::StencilOp back_zfail = regs.stencil_back_op_zfail;
        const Maxwell::StencilOp back_zpass = regs.stencil_back_op_zpass;
        const Maxwell::ComparisonOp back_compare = regs.stencil_back_func_func;
        scheduler.Record([fail, zfail, zpass, compare, back_fail, back_zfail, back_zpass,
                          back_compare](vk::CommandBuffer cmdbuf) {
            cmdbuf.SetStencilOpEXT(VK_STENCIL_FACE_FRONT_BIT, MaxwellToVK::StencilOp(fail),
                                   MaxwellToVK::StencilOp(zpass), MaxwellToVK::StencilOp(zfail),
                                   MaxwellToVK::ComparisonOp(compare));
            cmdbuf.SetStencilOpEXT(VK_STENCIL_FACE_BACK_BIT, MaxwellToVK::StencilOp(back_fail),
                                   MaxwellToVK::StencilOp(back_zpass),
                                   MaxwellToVK::StencilOp(back_zfail),
                                   MaxwellToVK::ComparisonOp(back_compare));
        });
    }
}

void RasterizerVulkan::UpdateStencilTestEnable(Tegra::Engines::Maxwell3D::Regs& regs) {
    if (!state_tracker.TouchStencilTestEnable()) {
        return;
    }
    scheduler.Record([enable = regs.stencil_enable](vk::CommandBuffer cmdbuf) {
        cmdbuf.SetStencilTestEnableEXT(enable);
    });
}

} // namespace Vulkan
