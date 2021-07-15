// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <boost/container/small_vector.hpp>

#include "common/alignment.h"
#include "common/common_types.h"
#include "common/literals.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "video_core/compatible_formats.h"
#include "video_core/delayed_destruction_ring.h"
#include "video_core/dirty_flags.h"
#include "video_core/engines/fermi_2d.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/memory_manager.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/surface.h"
#include "video_core/texture_cache/descriptor_table.h"
#include "video_core/texture_cache/format_lookup_table.h"
#include "video_core/texture_cache/formatter.h"
#include "video_core/texture_cache/image_base.h"
#include "video_core/texture_cache/image_info.h"
#include "video_core/texture_cache/image_view_base.h"
#include "video_core/texture_cache/image_view_info.h"
#include "video_core/texture_cache/render_targets.h"
#include "video_core/texture_cache/samples_helper.h"
#include "video_core/texture_cache/slot_vector.h"
#include "video_core/texture_cache/types.h"
#include "video_core/texture_cache/util.h"
#include "video_core/textures/texture.h"

namespace VideoCommon {

using Tegra::Texture::SwizzleSource;
using Tegra::Texture::TextureType;
using Tegra::Texture::TICEntry;
using Tegra::Texture::TSCEntry;
using VideoCore::Surface::GetFormatType;
using VideoCore::Surface::IsCopyCompatible;
using VideoCore::Surface::PixelFormat;
using VideoCore::Surface::PixelFormatFromDepthFormat;
using VideoCore::Surface::PixelFormatFromRenderTargetFormat;
using VideoCore::Surface::SurfaceType;
using namespace Common::Literals;

template <class P>
class TextureCache {
    /// Address shift for caching images into a hash table
    static constexpr u64 PAGE_BITS = 20;

    /// Enables debugging features to the texture cache
    static constexpr bool ENABLE_VALIDATION = P::ENABLE_VALIDATION;
    /// Implement blits as copies between framebuffers
    static constexpr bool FRAMEBUFFER_BLITS = P::FRAMEBUFFER_BLITS;
    /// True when some copies have to be emulated
    static constexpr bool HAS_EMULATED_COPIES = P::HAS_EMULATED_COPIES;
    /// True when the API can provide info about the memory of the device.
    static constexpr bool HAS_DEVICE_MEMORY_INFO = P::HAS_DEVICE_MEMORY_INFO;

    /// Image view ID for null descriptors
    static constexpr ImageViewId NULL_IMAGE_VIEW_ID{0};
    /// Sampler ID for bugged sampler ids
    static constexpr SamplerId NULL_SAMPLER_ID{0};

    static constexpr u64 DEFAULT_EXPECTED_MEMORY = 1_GiB;
    static constexpr u64 DEFAULT_CRITICAL_MEMORY = 2_GiB;

    using Runtime = typename P::Runtime;
    using Image = typename P::Image;
    using ImageAlloc = typename P::ImageAlloc;
    using ImageView = typename P::ImageView;
    using Sampler = typename P::Sampler;
    using Framebuffer = typename P::Framebuffer;

    struct BlitImages {
        ImageId dst_id;
        ImageId src_id;
        PixelFormat dst_format;
        PixelFormat src_format;
    };

    template <typename T>
    struct IdentityHash {
        [[nodiscard]] size_t operator()(T value) const noexcept {
            return static_cast<size_t>(value);
        }
    };

public:
    explicit TextureCache(Runtime&, VideoCore::RasterizerInterface&, Tegra::Engines::Maxwell3D&,
                          Tegra::Engines::KeplerCompute&, Tegra::MemoryManager&);

    /// Notify the cache that a new frame has been queued
    void TickFrame();

    /// Return a constant reference to the given image view id
    [[nodiscard]] const ImageView& GetImageView(ImageViewId id) const noexcept;

    /// Return a reference to the given image view id
    [[nodiscard]] ImageView& GetImageView(ImageViewId id) noexcept;

    /// Mark an image as modified from the GPU
    void MarkModification(ImageId id) noexcept;

    /// Fill image_view_ids with the graphics images in indices
    void FillGraphicsImageViews(std::span<const u32> indices,
                                std::span<ImageViewId> image_view_ids);

    /// Fill image_view_ids with the compute images in indices
    void FillComputeImageViews(std::span<const u32> indices, std::span<ImageViewId> image_view_ids);

    /// Get the sampler from the graphics descriptor table in the specified index
    Sampler* GetGraphicsSampler(u32 index);

    /// Get the sampler from the compute descriptor table in the specified index
    Sampler* GetComputeSampler(u32 index);

    /// Refresh the state for graphics image view and sampler descriptors
    void SynchronizeGraphicsDescriptors();

    /// Refresh the state for compute image view and sampler descriptors
    void SynchronizeComputeDescriptors();

    /// Update bound render targets and upload memory if necessary
    /// @param is_clear True when the render targets are being used for clears
    void UpdateRenderTargets(bool is_clear);

    /// Find a framebuffer with the currently bound render targets
    /// UpdateRenderTargets should be called before this
    Framebuffer* GetFramebuffer();

    /// Mark images in a range as modified from the CPU
    void WriteMemory(VAddr cpu_addr, size_t size);

    /// Download contents of host images to guest memory in a region
    void DownloadMemory(VAddr cpu_addr, size_t size);

    /// Remove images in a region
    void UnmapMemory(VAddr cpu_addr, size_t size);

    /// Remove images in a region
    void UnmapGPUMemory(GPUVAddr gpu_addr, size_t size);

    /// Blit an image with the given parameters
    void BlitImage(const Tegra::Engines::Fermi2D::Surface& dst,
                   const Tegra::Engines::Fermi2D::Surface& src,
                   const Tegra::Engines::Fermi2D::Config& copy);

    /// Invalidate the contents of the color buffer index
    /// These contents become unspecified, the cache can assume aggressive optimizations.
    void InvalidateColorBuffer(size_t index);

    /// Invalidate the contents of the depth buffer
    /// These contents become unspecified, the cache can assume aggressive optimizations.
    void InvalidateDepthBuffer();

    /// Try to find a cached image view in the given CPU address
    [[nodiscard]] ImageView* TryFindFramebufferImageView(VAddr cpu_addr);

    /// Return true when there are uncommitted images to be downloaded
    [[nodiscard]] bool HasUncommittedFlushes() const noexcept;

    /// Return true when the caller should wait for async downloads
    [[nodiscard]] bool ShouldWaitAsyncFlushes() const noexcept;

    /// Commit asynchronous downloads
    void CommitAsyncFlushes();

    /// Pop asynchronous downloads
    void PopAsyncFlushes();

    /// Return true when a CPU region is modified from the GPU
    [[nodiscard]] bool IsRegionGpuModified(VAddr addr, size_t size);

    std::mutex mutex;

private:
    /// Iterate over all page indices in a range
    template <typename Func>
    static void ForEachCPUPage(VAddr addr, size_t size, Func&& func) {
        static constexpr bool RETURNS_BOOL = std::is_same_v<std::invoke_result<Func, u64>, bool>;
        const u64 page_end = (addr + size - 1) >> PAGE_BITS;
        for (u64 page = addr >> PAGE_BITS; page <= page_end; ++page) {
            if constexpr (RETURNS_BOOL) {
                if (func(page)) {
                    break;
                }
            } else {
                func(page);
            }
        }
    }

    template <typename Func>
    static void ForEachGPUPage(GPUVAddr addr, size_t size, Func&& func) {
        static constexpr bool RETURNS_BOOL = std::is_same_v<std::invoke_result<Func, u64>, bool>;
        const u64 page_end = (addr + size - 1) >> PAGE_BITS;
        for (u64 page = addr >> PAGE_BITS; page <= page_end; ++page) {
            if constexpr (RETURNS_BOOL) {
                if (func(page)) {
                    break;
                }
            } else {
                func(page);
            }
        }
    }

    /// Runs the Garbage Collector.
    void RunGarbageCollector();

    /// Fills image_view_ids in the image views in indices
    void FillImageViews(DescriptorTable<TICEntry>& table,
                        std::span<ImageViewId> cached_image_view_ids, std::span<const u32> indices,
                        std::span<ImageViewId> image_view_ids);

    /// Find or create an image view in the guest descriptor table
    ImageViewId VisitImageView(DescriptorTable<TICEntry>& table,
                               std::span<ImageViewId> cached_image_view_ids, u32 index);

    /// Find or create a framebuffer with the given render target parameters
    FramebufferId GetFramebufferId(const RenderTargets& key);

    /// Refresh the contents (pixel data) of an image
    void RefreshContents(Image& image, ImageId image_id);

    /// Upload data from guest to an image
    template <typename StagingBuffer>
    void UploadImageContents(Image& image, StagingBuffer& staging_buffer);

    /// Find or create an image view from a guest descriptor
    [[nodiscard]] ImageViewId FindImageView(const TICEntry& config);

    /// Create a new image view from a guest descriptor
    [[nodiscard]] ImageViewId CreateImageView(const TICEntry& config);

    /// Find or create an image from the given parameters
    [[nodiscard]] ImageId FindOrInsertImage(const ImageInfo& info, GPUVAddr gpu_addr,
                                            RelaxedOptions options = RelaxedOptions{});

    /// Find an image from the given parameters
    [[nodiscard]] ImageId FindImage(const ImageInfo& info, GPUVAddr gpu_addr,
                                    RelaxedOptions options);

    /// Create an image from the given parameters
    [[nodiscard]] ImageId InsertImage(const ImageInfo& info, GPUVAddr gpu_addr,
                                      RelaxedOptions options);

    /// Create a new image and join perfectly matching existing images
    /// Remove joined images from the cache
    [[nodiscard]] ImageId JoinImages(const ImageInfo& info, GPUVAddr gpu_addr, VAddr cpu_addr);

    /// Return a blit image pair from the given guest blit parameters
    [[nodiscard]] BlitImages GetBlitImages(const Tegra::Engines::Fermi2D::Surface& dst,
                                           const Tegra::Engines::Fermi2D::Surface& src);

    /// Find or create a sampler from a guest descriptor sampler
    [[nodiscard]] SamplerId FindSampler(const TSCEntry& config);

    /// Find or create an image view for the given color buffer index
    [[nodiscard]] ImageViewId FindColorBuffer(size_t index, bool is_clear);

    /// Find or create an image view for the depth buffer
    [[nodiscard]] ImageViewId FindDepthBuffer(bool is_clear);

    /// Find or create a view for a render target with the given image parameters
    [[nodiscard]] ImageViewId FindRenderTargetView(const ImageInfo& info, GPUVAddr gpu_addr,
                                                   bool is_clear);

    /// Iterates over all the images in a region calling func
    template <typename Func>
    void ForEachImageInRegion(VAddr cpu_addr, size_t size, Func&& func);

    template <typename Func>
    void ForEachImageInRegionGPU(GPUVAddr gpu_addr, size_t size, Func&& func);

    template <typename Func>
    void ForEachSparseImageInRegion(GPUVAddr gpu_addr, size_t size, Func&& func);

    /// Iterates over all the images in a region calling func
    template <typename Func>
    void ForEachSparseSegment(ImageBase& image, Func&& func);

    /// Find or create an image view in the given image with the passed parameters
    [[nodiscard]] ImageViewId FindOrEmplaceImageView(ImageId image_id, const ImageViewInfo& info);

    /// Register image in the page table
    void RegisterImage(ImageId image);

    /// Unregister image from the page table
    void UnregisterImage(ImageId image);

    /// Track CPU reads and writes for image
    void TrackImage(ImageBase& image, ImageId image_id);

    /// Stop tracking CPU reads and writes for image
    void UntrackImage(ImageBase& image, ImageId image_id);

    /// Delete image from the cache
    void DeleteImage(ImageId image);

    /// Remove image views references from the cache
    void RemoveImageViewReferences(std::span<const ImageViewId> removed_views);

    /// Remove framebuffers using the given image views from the cache
    void RemoveFramebuffers(std::span<const ImageViewId> removed_views);

    /// Mark an image as modified from the GPU
    void MarkModification(ImageBase& image) noexcept;

    /// Synchronize image aliases, copying data if needed
    void SynchronizeAliases(ImageId image_id);

    /// Prepare an image to be used
    void PrepareImage(ImageId image_id, bool is_modification, bool invalidate);

    /// Prepare an image view to be used
    void PrepareImageView(ImageViewId image_view_id, bool is_modification, bool invalidate);

    /// Execute copies from one image to the other, even if they are incompatible
    void CopyImage(ImageId dst_id, ImageId src_id, std::span<const ImageCopy> copies);

    /// Bind an image view as render target, downloading resources preemtively if needed
    void BindRenderTarget(ImageViewId* old_id, ImageViewId new_id);

    /// Create a render target from a given image and image view parameters
    [[nodiscard]] std::pair<FramebufferId, ImageViewId> RenderTargetFromImage(
        ImageId, const ImageViewInfo& view_info);

    /// Returns true if the current clear parameters clear the whole image of a given image view
    [[nodiscard]] bool IsFullClear(ImageViewId id);

    Runtime& runtime;
    VideoCore::RasterizerInterface& rasterizer;
    Tegra::Engines::Maxwell3D& maxwell3d;
    Tegra::Engines::KeplerCompute& kepler_compute;
    Tegra::MemoryManager& gpu_memory;

    DescriptorTable<TICEntry> graphics_image_table{gpu_memory};
    DescriptorTable<TSCEntry> graphics_sampler_table{gpu_memory};
    std::vector<SamplerId> graphics_sampler_ids;
    std::vector<ImageViewId> graphics_image_view_ids;

    DescriptorTable<TICEntry> compute_image_table{gpu_memory};
    DescriptorTable<TSCEntry> compute_sampler_table{gpu_memory};
    std::vector<SamplerId> compute_sampler_ids;
    std::vector<ImageViewId> compute_image_view_ids;

    RenderTargets render_targets;

    std::unordered_map<TICEntry, ImageViewId> image_views;
    std::unordered_map<TSCEntry, SamplerId> samplers;
    std::unordered_map<RenderTargets, FramebufferId> framebuffers;

    std::unordered_map<u64, std::vector<ImageMapId>, IdentityHash<u64>> page_table;
    std::unordered_map<u64, std::vector<ImageId>, IdentityHash<u64>> gpu_page_table;
    std::unordered_map<u64, std::vector<ImageId>, IdentityHash<u64>> sparse_page_table;

    std::unordered_map<ImageId, std::vector<ImageViewId>> sparse_views;

    VAddr virtual_invalid_space{};

    bool has_deleted_images = false;
    u64 total_used_memory = 0;
    u64 minimum_memory;
    u64 expected_memory;
    u64 critical_memory;

    SlotVector<Image> slot_images;
    SlotVector<ImageMapView> slot_map_views;
    SlotVector<ImageView> slot_image_views;
    SlotVector<ImageAlloc> slot_image_allocs;
    SlotVector<Sampler> slot_samplers;
    SlotVector<Framebuffer> slot_framebuffers;

    // TODO: This data structure is not optimal and it should be reworked
    std::vector<ImageId> uncommitted_downloads;
    std::queue<std::vector<ImageId>> committed_downloads;

    static constexpr size_t TICKS_TO_DESTROY = 6;
    DelayedDestructionRing<Image, TICKS_TO_DESTROY> sentenced_images;
    DelayedDestructionRing<ImageView, TICKS_TO_DESTROY> sentenced_image_view;
    DelayedDestructionRing<Framebuffer, TICKS_TO_DESTROY> sentenced_framebuffers;

    std::unordered_map<GPUVAddr, ImageAllocId> image_allocs_table;

    u64 modification_tick = 0;
    u64 frame_tick = 0;
    typename SlotVector<Image>::Iterator deletion_iterator;
};

template <class P>
TextureCache<P>::TextureCache(Runtime& runtime_, VideoCore::RasterizerInterface& rasterizer_,
                              Tegra::Engines::Maxwell3D& maxwell3d_,
                              Tegra::Engines::KeplerCompute& kepler_compute_,
                              Tegra::MemoryManager& gpu_memory_)
    : runtime{runtime_}, rasterizer{rasterizer_}, maxwell3d{maxwell3d_},
      kepler_compute{kepler_compute_}, gpu_memory{gpu_memory_} {
    // Configure null sampler
    TSCEntry sampler_descriptor{};
    sampler_descriptor.min_filter.Assign(Tegra::Texture::TextureFilter::Linear);
    sampler_descriptor.mag_filter.Assign(Tegra::Texture::TextureFilter::Linear);
    sampler_descriptor.mipmap_filter.Assign(Tegra::Texture::TextureMipmapFilter::Linear);
    sampler_descriptor.cubemap_anisotropy.Assign(1);

    // Make sure the first index is reserved for the null resources
    // This way the null resource becomes a compile time constant
    void(slot_image_views.insert(runtime, NullImageParams{}));
    void(slot_samplers.insert(runtime, sampler_descriptor));

    deletion_iterator = slot_images.begin();

    if constexpr (HAS_DEVICE_MEMORY_INFO) {
        const auto device_memory = runtime.GetDeviceLocalMemory();
        const u64 possible_expected_memory = (device_memory * 3) / 10;
        const u64 possible_critical_memory = (device_memory * 6) / 10;
        expected_memory = std::max(possible_expected_memory, DEFAULT_EXPECTED_MEMORY);
        critical_memory = std::max(possible_critical_memory, DEFAULT_CRITICAL_MEMORY);
        minimum_memory = 0;
    } else {
        // on OGL we can be more conservatives as the driver takes care.
        expected_memory = DEFAULT_EXPECTED_MEMORY + 512_MiB;
        critical_memory = DEFAULT_CRITICAL_MEMORY + 1_GiB;
        minimum_memory = expected_memory;
    }
}

template <class P>
void TextureCache<P>::RunGarbageCollector() {
    const bool high_priority_mode = total_used_memory >= expected_memory;
    const bool aggressive_mode = total_used_memory >= critical_memory;
    const u64 ticks_to_destroy = high_priority_mode ? 60 : 100;
    int num_iterations = aggressive_mode ? 256 : (high_priority_mode ? 128 : 64);
    for (; num_iterations > 0; --num_iterations) {
        if (deletion_iterator == slot_images.end()) {
            deletion_iterator = slot_images.begin();
            if (deletion_iterator == slot_images.end()) {
                break;
            }
        }
        auto [image_id, image_tmp] = *deletion_iterator;
        Image* image = image_tmp; // fix clang error.
        const bool is_alias = True(image->flags & ImageFlagBits::Alias);
        const bool is_bad_overlap = True(image->flags & ImageFlagBits::BadOverlap);
        const bool must_download = image->IsSafeDownload();
        bool should_care = is_bad_overlap || is_alias || (high_priority_mode && !must_download);
        const u64 ticks_needed =
            is_bad_overlap
                ? ticks_to_destroy >> 4
                : ((should_care && aggressive_mode) ? ticks_to_destroy >> 1 : ticks_to_destroy);
        should_care |= aggressive_mode;
        if (should_care && image->frame_tick + ticks_needed < frame_tick) {
            if (is_bad_overlap) {
                const bool overlap_check = std::ranges::all_of(
                    image->overlapping_images, [&, image](const ImageId& overlap_id) {
                        auto& overlap = slot_images[overlap_id];
                        return overlap.frame_tick >= image->frame_tick;
                    });
                if (!overlap_check) {
                    ++deletion_iterator;
                    continue;
                }
            }
            if (!is_bad_overlap && must_download) {
                const bool alias_check = std::ranges::none_of(
                    image->aliased_images, [&, image](const AliasedImage& alias) {
                        auto& alias_image = slot_images[alias.id];
                        return (alias_image.frame_tick < image->frame_tick) ||
                               (alias_image.modification_tick < image->modification_tick);
                    });

                if (alias_check) {
                    auto map = runtime.DownloadStagingBuffer(image->unswizzled_size_bytes);
                    const auto copies = FullDownloadCopies(image->info);
                    image->DownloadMemory(map, copies);
                    runtime.Finish();
                    SwizzleImage(gpu_memory, image->gpu_addr, image->info, copies, map.mapped_span);
                }
            }
            if (True(image->flags & ImageFlagBits::Tracked)) {
                UntrackImage(*image, image_id);
            }
            UnregisterImage(image_id);
            DeleteImage(image_id);
            if (is_bad_overlap) {
                ++num_iterations;
            }
        }
        ++deletion_iterator;
    }
}

template <class P>
void TextureCache<P>::TickFrame() {
    if (Settings::values.use_caches_gc.GetValue() && total_used_memory > minimum_memory) {
        RunGarbageCollector();
    }
    sentenced_images.Tick();
    sentenced_framebuffers.Tick();
    sentenced_image_view.Tick();
    ++frame_tick;
}

template <class P>
const typename P::ImageView& TextureCache<P>::GetImageView(ImageViewId id) const noexcept {
    return slot_image_views[id];
}

template <class P>
typename P::ImageView& TextureCache<P>::GetImageView(ImageViewId id) noexcept {
    return slot_image_views[id];
}

template <class P>
void TextureCache<P>::MarkModification(ImageId id) noexcept {
    MarkModification(slot_images[id]);
}

template <class P>
void TextureCache<P>::FillGraphicsImageViews(std::span<const u32> indices,
                                             std::span<ImageViewId> image_view_ids) {
    FillImageViews(graphics_image_table, graphics_image_view_ids, indices, image_view_ids);
}

template <class P>
void TextureCache<P>::FillComputeImageViews(std::span<const u32> indices,
                                            std::span<ImageViewId> image_view_ids) {
    FillImageViews(compute_image_table, compute_image_view_ids, indices, image_view_ids);
}

template <class P>
typename P::Sampler* TextureCache<P>::GetGraphicsSampler(u32 index) {
    if (index > graphics_sampler_table.Limit()) {
        LOG_DEBUG(HW_GPU, "Invalid sampler index={}", index);
        return &slot_samplers[NULL_SAMPLER_ID];
    }
    const auto [descriptor, is_new] = graphics_sampler_table.Read(index);
    SamplerId& id = graphics_sampler_ids[index];
    if (is_new) {
        id = FindSampler(descriptor);
    }
    return &slot_samplers[id];
}

template <class P>
typename P::Sampler* TextureCache<P>::GetComputeSampler(u32 index) {
    if (index > compute_sampler_table.Limit()) {
        LOG_DEBUG(HW_GPU, "Invalid sampler index={}", index);
        return &slot_samplers[NULL_SAMPLER_ID];
    }
    const auto [descriptor, is_new] = compute_sampler_table.Read(index);
    SamplerId& id = compute_sampler_ids[index];
    if (is_new) {
        id = FindSampler(descriptor);
    }
    return &slot_samplers[id];
}

template <class P>
void TextureCache<P>::SynchronizeGraphicsDescriptors() {
    using SamplerIndex = Tegra::Engines::Maxwell3D::Regs::SamplerIndex;
    const bool linked_tsc = maxwell3d.regs.sampler_index == SamplerIndex::ViaHeaderIndex;
    const u32 tic_limit = maxwell3d.regs.tic.limit;
    const u32 tsc_limit = linked_tsc ? tic_limit : maxwell3d.regs.tsc.limit;
    if (graphics_sampler_table.Synchornize(maxwell3d.regs.tsc.Address(), tsc_limit)) {
        graphics_sampler_ids.resize(tsc_limit + 1, CORRUPT_ID);
    }
    if (graphics_image_table.Synchornize(maxwell3d.regs.tic.Address(), tic_limit)) {
        graphics_image_view_ids.resize(tic_limit + 1, CORRUPT_ID);
    }
}

template <class P>
void TextureCache<P>::SynchronizeComputeDescriptors() {
    const bool linked_tsc = kepler_compute.launch_description.linked_tsc;
    const u32 tic_limit = kepler_compute.regs.tic.limit;
    const u32 tsc_limit = linked_tsc ? tic_limit : kepler_compute.regs.tsc.limit;
    const GPUVAddr tsc_gpu_addr = kepler_compute.regs.tsc.Address();
    if (compute_sampler_table.Synchornize(tsc_gpu_addr, tsc_limit)) {
        compute_sampler_ids.resize(tsc_limit + 1, CORRUPT_ID);
    }
    if (compute_image_table.Synchornize(kepler_compute.regs.tic.Address(), tic_limit)) {
        compute_image_view_ids.resize(tic_limit + 1, CORRUPT_ID);
    }
}

template <class P>
void TextureCache<P>::UpdateRenderTargets(bool is_clear) {
    using namespace VideoCommon::Dirty;
    auto& flags = maxwell3d.dirty.flags;
    if (!flags[Dirty::RenderTargets]) {
        for (size_t index = 0; index < NUM_RT; ++index) {
            ImageViewId& color_buffer_id = render_targets.color_buffer_ids[index];
            PrepareImageView(color_buffer_id, true, is_clear && IsFullClear(color_buffer_id));
        }
        const ImageViewId depth_buffer_id = render_targets.depth_buffer_id;
        PrepareImageView(depth_buffer_id, true, is_clear && IsFullClear(depth_buffer_id));
        return;
    }
    flags[Dirty::RenderTargets] = false;

    // Render target control is used on all render targets, so force look ups when this one is up
    const bool force = flags[Dirty::RenderTargetControl];
    flags[Dirty::RenderTargetControl] = false;

    for (size_t index = 0; index < NUM_RT; ++index) {
        ImageViewId& color_buffer_id = render_targets.color_buffer_ids[index];
        if (flags[Dirty::ColorBuffer0 + index] || force) {
            flags[Dirty::ColorBuffer0 + index] = false;
            BindRenderTarget(&color_buffer_id, FindColorBuffer(index, is_clear));
        }
        PrepareImageView(color_buffer_id, true, is_clear && IsFullClear(color_buffer_id));
    }
    if (flags[Dirty::ZetaBuffer] || force) {
        flags[Dirty::ZetaBuffer] = false;
        BindRenderTarget(&render_targets.depth_buffer_id, FindDepthBuffer(is_clear));
    }
    const ImageViewId depth_buffer_id = render_targets.depth_buffer_id;
    PrepareImageView(depth_buffer_id, true, is_clear && IsFullClear(depth_buffer_id));

    for (size_t index = 0; index < NUM_RT; ++index) {
        render_targets.draw_buffers[index] = static_cast<u8>(maxwell3d.regs.rt_control.Map(index));
    }
    render_targets.size = Extent2D{
        maxwell3d.regs.render_area.width,
        maxwell3d.regs.render_area.height,
    };
}

template <class P>
typename P::Framebuffer* TextureCache<P>::GetFramebuffer() {
    return &slot_framebuffers[GetFramebufferId(render_targets)];
}

template <class P>
void TextureCache<P>::FillImageViews(DescriptorTable<TICEntry>& table,
                                     std::span<ImageViewId> cached_image_view_ids,
                                     std::span<const u32> indices,
                                     std::span<ImageViewId> image_view_ids) {
    ASSERT(indices.size() <= image_view_ids.size());
    do {
        has_deleted_images = false;
        std::ranges::transform(indices, image_view_ids.begin(), [&](u32 index) {
            return VisitImageView(table, cached_image_view_ids, index);
        });
    } while (has_deleted_images);
}

template <class P>
ImageViewId TextureCache<P>::VisitImageView(DescriptorTable<TICEntry>& table,
                                            std::span<ImageViewId> cached_image_view_ids,
                                            u32 index) {
    if (index > table.Limit()) {
        LOG_DEBUG(HW_GPU, "Invalid image view index={}", index);
        return NULL_IMAGE_VIEW_ID;
    }
    const auto [descriptor, is_new] = table.Read(index);
    ImageViewId& image_view_id = cached_image_view_ids[index];
    if (is_new) {
        image_view_id = FindImageView(descriptor);
    }
    if (image_view_id != NULL_IMAGE_VIEW_ID) {
        PrepareImageView(image_view_id, false, false);
    }
    return image_view_id;
}

template <class P>
FramebufferId TextureCache<P>::GetFramebufferId(const RenderTargets& key) {
    const auto [pair, is_new] = framebuffers.try_emplace(key);
    FramebufferId& framebuffer_id = pair->second;
    if (!is_new) {
        return framebuffer_id;
    }
    std::array<ImageView*, NUM_RT> color_buffers;
    std::ranges::transform(key.color_buffer_ids, color_buffers.begin(),
                           [this](ImageViewId id) { return id ? &slot_image_views[id] : nullptr; });
    ImageView* const depth_buffer =
        key.depth_buffer_id ? &slot_image_views[key.depth_buffer_id] : nullptr;
    framebuffer_id = slot_framebuffers.insert(runtime, color_buffers, depth_buffer, key);
    return framebuffer_id;
}

template <class P>
void TextureCache<P>::WriteMemory(VAddr cpu_addr, size_t size) {
    ForEachImageInRegion(cpu_addr, size, [this](ImageId image_id, Image& image) {
        if (True(image.flags & ImageFlagBits::CpuModified)) {
            return;
        }
        image.flags |= ImageFlagBits::CpuModified;
        if (True(image.flags & ImageFlagBits::Tracked)) {
            UntrackImage(image, image_id);
        }
    });
}

template <class P>
void TextureCache<P>::DownloadMemory(VAddr cpu_addr, size_t size) {
    std::vector<ImageId> images;
    ForEachImageInRegion(cpu_addr, size, [this, &images](ImageId image_id, ImageBase& image) {
        if (!image.IsSafeDownload()) {
            return;
        }
        image.flags &= ~ImageFlagBits::GpuModified;
        images.push_back(image_id);
    });
    if (images.empty()) {
        return;
    }
    std::ranges::sort(images, [this](ImageId lhs, ImageId rhs) {
        return slot_images[lhs].modification_tick < slot_images[rhs].modification_tick;
    });
    for (const ImageId image_id : images) {
        Image& image = slot_images[image_id];
        auto map = runtime.DownloadStagingBuffer(image.unswizzled_size_bytes);
        const auto copies = FullDownloadCopies(image.info);
        image.DownloadMemory(map, copies);
        runtime.Finish();
        SwizzleImage(gpu_memory, image.gpu_addr, image.info, copies, map.mapped_span);
    }
}

template <class P>
void TextureCache<P>::UnmapMemory(VAddr cpu_addr, size_t size) {
    std::vector<ImageId> deleted_images;
    ForEachImageInRegion(cpu_addr, size, [&](ImageId id, Image&) { deleted_images.push_back(id); });
    for (const ImageId id : deleted_images) {
        Image& image = slot_images[id];
        if (True(image.flags & ImageFlagBits::Tracked)) {
            UntrackImage(image, id);
        }
        UnregisterImage(id);
        DeleteImage(id);
    }
}

template <class P>
void TextureCache<P>::UnmapGPUMemory(GPUVAddr gpu_addr, size_t size) {
    std::vector<ImageId> deleted_images;
    ForEachImageInRegionGPU(gpu_addr, size,
                            [&](ImageId id, Image&) { deleted_images.push_back(id); });
    for (const ImageId id : deleted_images) {
        Image& image = slot_images[id];
        if (True(image.flags & ImageFlagBits::Remapped)) {
            continue;
        }
        image.flags |= ImageFlagBits::Remapped;
        if (True(image.flags & ImageFlagBits::Tracked)) {
            UntrackImage(image, id);
        }
    }
}

template <class P>
void TextureCache<P>::BlitImage(const Tegra::Engines::Fermi2D::Surface& dst,
                                const Tegra::Engines::Fermi2D::Surface& src,
                                const Tegra::Engines::Fermi2D::Config& copy) {
    const BlitImages images = GetBlitImages(dst, src);
    const ImageId dst_id = images.dst_id;
    const ImageId src_id = images.src_id;
    PrepareImage(src_id, false, false);
    PrepareImage(dst_id, true, false);

    ImageBase& dst_image = slot_images[dst_id];
    const ImageBase& src_image = slot_images[src_id];

    // TODO: Deduplicate
    const std::optional src_base = src_image.TryFindBase(src.Address());
    const SubresourceRange src_range{.base = src_base.value(), .extent = {1, 1}};
    const ImageViewInfo src_view_info(ImageViewType::e2D, images.src_format, src_range);
    const auto [src_framebuffer_id, src_view_id] = RenderTargetFromImage(src_id, src_view_info);
    const auto [src_samples_x, src_samples_y] = SamplesLog2(src_image.info.num_samples);
    const Region2D src_region{
        Offset2D{.x = copy.src_x0 >> src_samples_x, .y = copy.src_y0 >> src_samples_y},
        Offset2D{.x = copy.src_x1 >> src_samples_x, .y = copy.src_y1 >> src_samples_y},
    };

    const std::optional dst_base = dst_image.TryFindBase(dst.Address());
    const SubresourceRange dst_range{.base = dst_base.value(), .extent = {1, 1}};
    const ImageViewInfo dst_view_info(ImageViewType::e2D, images.dst_format, dst_range);
    const auto [dst_framebuffer_id, dst_view_id] = RenderTargetFromImage(dst_id, dst_view_info);
    const auto [dst_samples_x, dst_samples_y] = SamplesLog2(dst_image.info.num_samples);
    const Region2D dst_region{
        Offset2D{.x = copy.dst_x0 >> dst_samples_x, .y = copy.dst_y0 >> dst_samples_y},
        Offset2D{.x = copy.dst_x1 >> dst_samples_x, .y = copy.dst_y1 >> dst_samples_y},
    };

    // Always call this after src_framebuffer_id was queried, as the address might be invalidated.
    Framebuffer* const dst_framebuffer = &slot_framebuffers[dst_framebuffer_id];
    if constexpr (FRAMEBUFFER_BLITS) {
        // OpenGL blits from framebuffers, not images
        Framebuffer* const src_framebuffer = &slot_framebuffers[src_framebuffer_id];
        runtime.BlitFramebuffer(dst_framebuffer, src_framebuffer, dst_region, src_region,
                                copy.filter, copy.operation);
    } else {
        // Vulkan can blit images, but it lacks format reinterpretations
        // Provide a framebuffer in case it's necessary
        ImageView& dst_view = slot_image_views[dst_view_id];
        ImageView& src_view = slot_image_views[src_view_id];
        runtime.BlitImage(dst_framebuffer, dst_view, src_view, dst_region, src_region, copy.filter,
                          copy.operation);
    }
}

template <class P>
void TextureCache<P>::InvalidateColorBuffer(size_t index) {
    ImageViewId& color_buffer_id = render_targets.color_buffer_ids[index];
    color_buffer_id = FindColorBuffer(index, false);
    if (!color_buffer_id) {
        LOG_ERROR(HW_GPU, "Invalidating invalid color buffer in index={}", index);
        return;
    }
    // When invalidating a color buffer, the old contents are no longer relevant
    ImageView& color_buffer = slot_image_views[color_buffer_id];
    Image& image = slot_images[color_buffer.image_id];
    image.flags &= ~ImageFlagBits::CpuModified;
    image.flags &= ~ImageFlagBits::GpuModified;

    runtime.InvalidateColorBuffer(color_buffer, index);
}

template <class P>
void TextureCache<P>::InvalidateDepthBuffer() {
    ImageViewId& depth_buffer_id = render_targets.depth_buffer_id;
    depth_buffer_id = FindDepthBuffer(false);
    if (!depth_buffer_id) {
        LOG_ERROR(HW_GPU, "Invalidating invalid depth buffer");
        return;
    }
    // When invalidating the depth buffer, the old contents are no longer relevant
    ImageBase& image = slot_images[slot_image_views[depth_buffer_id].image_id];
    image.flags &= ~ImageFlagBits::CpuModified;
    image.flags &= ~ImageFlagBits::GpuModified;

    ImageView& depth_buffer = slot_image_views[depth_buffer_id];
    runtime.InvalidateDepthBuffer(depth_buffer);
}

template <class P>
typename P::ImageView* TextureCache<P>::TryFindFramebufferImageView(VAddr cpu_addr) {
    // TODO: Properly implement this
    const auto it = page_table.find(cpu_addr >> PAGE_BITS);
    if (it == page_table.end()) {
        return nullptr;
    }
    const auto& image_map_ids = it->second;
    for (const ImageMapId map_id : image_map_ids) {
        const ImageMapView& map = slot_map_views[map_id];
        const ImageBase& image = slot_images[map.image_id];
        if (image.cpu_addr != cpu_addr) {
            continue;
        }
        if (image.image_view_ids.empty()) {
            continue;
        }
        return &slot_image_views[image.image_view_ids.at(0)];
    }
    return nullptr;
}

template <class P>
bool TextureCache<P>::HasUncommittedFlushes() const noexcept {
    return !uncommitted_downloads.empty();
}

template <class P>
bool TextureCache<P>::ShouldWaitAsyncFlushes() const noexcept {
    return !committed_downloads.empty() && !committed_downloads.front().empty();
}

template <class P>
void TextureCache<P>::CommitAsyncFlushes() {
    // This is intentionally passing the value by copy
    committed_downloads.push(uncommitted_downloads);
    uncommitted_downloads.clear();
}

template <class P>
void TextureCache<P>::PopAsyncFlushes() {
    if (committed_downloads.empty()) {
        return;
    }
    const std::span<const ImageId> download_ids = committed_downloads.front();
    if (download_ids.empty()) {
        committed_downloads.pop();
        return;
    }
    size_t total_size_bytes = 0;
    for (const ImageId image_id : download_ids) {
        total_size_bytes += slot_images[image_id].unswizzled_size_bytes;
    }
    auto download_map = runtime.DownloadStagingBuffer(total_size_bytes);
    const size_t original_offset = download_map.offset;
    for (const ImageId image_id : download_ids) {
        Image& image = slot_images[image_id];
        const auto copies = FullDownloadCopies(image.info);
        image.DownloadMemory(download_map, copies);
        download_map.offset += image.unswizzled_size_bytes;
    }
    // Wait for downloads to finish
    runtime.Finish();

    download_map.offset = original_offset;
    std::span<u8> download_span = download_map.mapped_span;
    for (const ImageId image_id : download_ids) {
        const ImageBase& image = slot_images[image_id];
        const auto copies = FullDownloadCopies(image.info);
        SwizzleImage(gpu_memory, image.gpu_addr, image.info, copies, download_span);
        download_map.offset += image.unswizzled_size_bytes;
        download_span = download_span.subspan(image.unswizzled_size_bytes);
    }
    committed_downloads.pop();
}

template <class P>
bool TextureCache<P>::IsRegionGpuModified(VAddr addr, size_t size) {
    bool is_modified = false;
    ForEachImageInRegion(addr, size, [&is_modified](ImageId, ImageBase& image) {
        if (False(image.flags & ImageFlagBits::GpuModified)) {
            return false;
        }
        is_modified = true;
        return true;
    });
    return is_modified;
}

template <class P>
void TextureCache<P>::RefreshContents(Image& image, ImageId image_id) {
    if (False(image.flags & ImageFlagBits::CpuModified)) {
        // Only upload modified images
        return;
    }
    image.flags &= ~ImageFlagBits::CpuModified;
    TrackImage(image, image_id);

    if (image.info.num_samples > 1) {
        LOG_WARNING(HW_GPU, "MSAA image uploads are not implemented");
        return;
    }
    auto staging = runtime.UploadStagingBuffer(MapSizeBytes(image));
    UploadImageContents(image, staging);
    runtime.InsertUploadMemoryBarrier();
}

template <class P>
template <typename StagingBuffer>
void TextureCache<P>::UploadImageContents(Image& image, StagingBuffer& staging) {
    const std::span<u8> mapped_span = staging.mapped_span;
    const GPUVAddr gpu_addr = image.gpu_addr;

    if (True(image.flags & ImageFlagBits::AcceleratedUpload)) {
        gpu_memory.ReadBlockUnsafe(gpu_addr, mapped_span.data(), mapped_span.size_bytes());
        const auto uploads = FullUploadSwizzles(image.info);
        runtime.AccelerateImageUpload(image, staging, uploads);
    } else if (True(image.flags & ImageFlagBits::Converted)) {
        std::vector<u8> unswizzled_data(image.unswizzled_size_bytes);
        auto copies = UnswizzleImage(gpu_memory, gpu_addr, image.info, unswizzled_data);
        ConvertImage(unswizzled_data, image.info, mapped_span, copies);
        image.UploadMemory(staging, copies);
    } else {
        const auto copies = UnswizzleImage(gpu_memory, gpu_addr, image.info, mapped_span);
        image.UploadMemory(staging, copies);
    }
}

template <class P>
ImageViewId TextureCache<P>::FindImageView(const TICEntry& config) {
    if (!IsValidEntry(gpu_memory, config)) {
        return NULL_IMAGE_VIEW_ID;
    }
    const auto [pair, is_new] = image_views.try_emplace(config);
    ImageViewId& image_view_id = pair->second;
    if (is_new) {
        image_view_id = CreateImageView(config);
    }
    return image_view_id;
}

template <class P>
ImageViewId TextureCache<P>::CreateImageView(const TICEntry& config) {
    const ImageInfo info(config);
    if (info.type == ImageType::Buffer) {
        const ImageViewInfo view_info(config, 0);
        return slot_image_views.insert(runtime, info, view_info, config.Address());
    }
    const u32 layer_offset = config.BaseLayer() * info.layer_stride;
    const GPUVAddr image_gpu_addr = config.Address() - layer_offset;
    const ImageId image_id = FindOrInsertImage(info, image_gpu_addr);
    if (!image_id) {
        return NULL_IMAGE_VIEW_ID;
    }
    ImageBase& image = slot_images[image_id];
    const SubresourceBase base = image.TryFindBase(config.Address()).value();
    ASSERT(base.level == 0);
    const ImageViewInfo view_info(config, base.layer);
    const ImageViewId image_view_id = FindOrEmplaceImageView(image_id, view_info);
    ImageViewBase& image_view = slot_image_views[image_view_id];
    image_view.flags |= ImageViewFlagBits::Strong;
    image.flags |= ImageFlagBits::Strong;
    return image_view_id;
}

template <class P>
ImageId TextureCache<P>::FindOrInsertImage(const ImageInfo& info, GPUVAddr gpu_addr,
                                           RelaxedOptions options) {
    if (const ImageId image_id = FindImage(info, gpu_addr, options); image_id) {
        return image_id;
    }
    return InsertImage(info, gpu_addr, options);
}

template <class P>
ImageId TextureCache<P>::FindImage(const ImageInfo& info, GPUVAddr gpu_addr,
                                   RelaxedOptions options) {
    std::optional<VAddr> cpu_addr = gpu_memory.GpuToCpuAddress(gpu_addr);
    if (!cpu_addr) {
        cpu_addr = gpu_memory.GpuToCpuAddress(gpu_addr, CalculateGuestSizeInBytes(info));
        if (!cpu_addr) {
            return ImageId{};
        }
    }
    const bool broken_views = runtime.HasBrokenTextureViewFormats();
    const bool native_bgr = runtime.HasNativeBgr();
    ImageId image_id;
    const auto lambda = [&](ImageId existing_image_id, ImageBase& existing_image) {
        if (True(existing_image.flags & ImageFlagBits::Remapped)) {
            return false;
        }
        if (info.type == ImageType::Linear || existing_image.info.type == ImageType::Linear) {
            const bool strict_size = False(options & RelaxedOptions::Size) &&
                                     True(existing_image.flags & ImageFlagBits::Strong);
            const ImageInfo& existing = existing_image.info;
            if (existing_image.gpu_addr == gpu_addr && existing.type == info.type &&
                existing.pitch == info.pitch &&
                IsPitchLinearSameSize(existing, info, strict_size) &&
                IsViewCompatible(existing.format, info.format, broken_views, native_bgr)) {
                image_id = existing_image_id;
                return true;
            }
        } else if (IsSubresource(info, existing_image, gpu_addr, options, broken_views,
                                 native_bgr)) {
            image_id = existing_image_id;
            return true;
        }
        return false;
    };
    ForEachImageInRegion(*cpu_addr, CalculateGuestSizeInBytes(info), lambda);
    return image_id;
}

template <class P>
ImageId TextureCache<P>::InsertImage(const ImageInfo& info, GPUVAddr gpu_addr,
                                     RelaxedOptions options) {
    std::optional<VAddr> cpu_addr = gpu_memory.GpuToCpuAddress(gpu_addr);
    if (!cpu_addr) {
        const auto size = CalculateGuestSizeInBytes(info);
        cpu_addr = gpu_memory.GpuToCpuAddress(gpu_addr, size);
        if (!cpu_addr) {
            const VAddr fake_addr = ~(1ULL << 40ULL) + virtual_invalid_space;
            virtual_invalid_space += Common::AlignUp(size, 32);
            cpu_addr = std::optional<VAddr>(fake_addr);
        }
    }
    ASSERT_MSG(cpu_addr, "Tried to insert an image to an invalid gpu_addr=0x{:x}", gpu_addr);
    const ImageId image_id = JoinImages(info, gpu_addr, *cpu_addr);
    const Image& image = slot_images[image_id];
    // Using "image.gpu_addr" instead of "gpu_addr" is important because it might be different
    const auto [it, is_new] = image_allocs_table.try_emplace(image.gpu_addr);
    if (is_new) {
        it->second = slot_image_allocs.insert();
    }
    slot_image_allocs[it->second].images.push_back(image_id);
    return image_id;
}

template <class P>
ImageId TextureCache<P>::JoinImages(const ImageInfo& info, GPUVAddr gpu_addr, VAddr cpu_addr) {
    ImageInfo new_info = info;
    const size_t size_bytes = CalculateGuestSizeInBytes(new_info);
    const bool broken_views = runtime.HasBrokenTextureViewFormats();
    const bool native_bgr = runtime.HasNativeBgr();
    std::vector<ImageId> overlap_ids;
    std::unordered_set<ImageId> overlaps_found;
    std::vector<ImageId> left_aliased_ids;
    std::vector<ImageId> right_aliased_ids;
    std::unordered_set<ImageId> ignore_textures;
    std::vector<ImageId> bad_overlap_ids;
    const auto region_check = [&](ImageId overlap_id, ImageBase& overlap) {
        if (True(overlap.flags & ImageFlagBits::Remapped)) {
            ignore_textures.insert(overlap_id);
            return;
        }
        if (info.type == ImageType::Linear) {
            if (info.pitch == overlap.info.pitch && gpu_addr == overlap.gpu_addr) {
                // Alias linear images with the same pitch
                left_aliased_ids.push_back(overlap_id);
            }
            return;
        }
        overlaps_found.insert(overlap_id);
        static constexpr bool strict_size = true;
        const std::optional<OverlapResult> solution = ResolveOverlap(
            new_info, gpu_addr, cpu_addr, overlap, strict_size, broken_views, native_bgr);
        if (solution) {
            gpu_addr = solution->gpu_addr;
            cpu_addr = solution->cpu_addr;
            new_info.resources = solution->resources;
            overlap_ids.push_back(overlap_id);
            return;
        }
        static constexpr auto options = RelaxedOptions::Size | RelaxedOptions::Format;
        const ImageBase new_image_base(new_info, gpu_addr, cpu_addr);
        if (IsSubresource(new_info, overlap, gpu_addr, options, broken_views, native_bgr)) {
            left_aliased_ids.push_back(overlap_id);
            overlap.flags |= ImageFlagBits::Alias;
        } else if (IsSubresource(overlap.info, new_image_base, overlap.gpu_addr, options,
                                 broken_views, native_bgr)) {
            right_aliased_ids.push_back(overlap_id);
            overlap.flags |= ImageFlagBits::Alias;
        } else {
            bad_overlap_ids.push_back(overlap_id);
            overlap.flags |= ImageFlagBits::BadOverlap;
        }
    };
    ForEachImageInRegion(cpu_addr, size_bytes, region_check);
    const auto region_check_gpu = [&](ImageId overlap_id, ImageBase& overlap) {
        if (!overlaps_found.contains(overlap_id)) {
            if (True(overlap.flags & ImageFlagBits::Remapped)) {
                ignore_textures.insert(overlap_id);
            }
            if (overlap.gpu_addr == gpu_addr && overlap.guest_size_bytes == size_bytes) {
                ignore_textures.insert(overlap_id);
            }
        }
    };
    ForEachSparseImageInRegion(gpu_addr, size_bytes, region_check_gpu);
    const ImageId new_image_id = slot_images.insert(runtime, new_info, gpu_addr, cpu_addr);
    Image& new_image = slot_images[new_image_id];

    if (!gpu_memory.IsContinousRange(new_image.gpu_addr, new_image.guest_size_bytes)) {
        new_image.flags |= ImageFlagBits::Sparse;
    }

    for (const ImageId overlap_id : ignore_textures) {
        Image& overlap = slot_images[overlap_id];
        if (True(overlap.flags & ImageFlagBits::GpuModified)) {
            UNIMPLEMENTED();
        }
        if (True(overlap.flags & ImageFlagBits::Tracked)) {
            UntrackImage(overlap, overlap_id);
        }
        UnregisterImage(overlap_id);
        DeleteImage(overlap_id);
    }

    // TODO: Only upload what we need
    RefreshContents(new_image, new_image_id);

    for (const ImageId overlap_id : overlap_ids) {
        Image& overlap = slot_images[overlap_id];
        if (overlap.info.num_samples != new_image.info.num_samples) {
            LOG_WARNING(HW_GPU, "Copying between images with different samples is not implemented");
        } else {
            const SubresourceBase base = new_image.TryFindBase(overlap.gpu_addr).value();
            const auto copies = MakeShrinkImageCopies(new_info, overlap.info, base);
            runtime.CopyImage(new_image, overlap, copies);
        }
        if (True(overlap.flags & ImageFlagBits::Tracked)) {
            UntrackImage(overlap, overlap_id);
        }
        UnregisterImage(overlap_id);
        DeleteImage(overlap_id);
    }
    ImageBase& new_image_base = new_image;
    for (const ImageId aliased_id : right_aliased_ids) {
        ImageBase& aliased = slot_images[aliased_id];
        AddImageAlias(new_image_base, aliased, new_image_id, aliased_id);
        new_image.flags |= ImageFlagBits::Alias;
    }
    for (const ImageId aliased_id : left_aliased_ids) {
        ImageBase& aliased = slot_images[aliased_id];
        AddImageAlias(aliased, new_image_base, aliased_id, new_image_id);
        new_image.flags |= ImageFlagBits::Alias;
    }
    for (const ImageId aliased_id : bad_overlap_ids) {
        ImageBase& aliased = slot_images[aliased_id];
        aliased.overlapping_images.push_back(new_image_id);
        new_image.overlapping_images.push_back(aliased_id);
        new_image.flags |= ImageFlagBits::BadOverlap;
    }
    RegisterImage(new_image_id);
    return new_image_id;
}

template <class P>
typename TextureCache<P>::BlitImages TextureCache<P>::GetBlitImages(
    const Tegra::Engines::Fermi2D::Surface& dst, const Tegra::Engines::Fermi2D::Surface& src) {
    static constexpr auto FIND_OPTIONS = RelaxedOptions::Format | RelaxedOptions::Samples;
    const GPUVAddr dst_addr = dst.Address();
    const GPUVAddr src_addr = src.Address();
    ImageInfo dst_info(dst);
    ImageInfo src_info(src);
    ImageId dst_id;
    ImageId src_id;
    do {
        has_deleted_images = false;
        dst_id = FindImage(dst_info, dst_addr, FIND_OPTIONS);
        src_id = FindImage(src_info, src_addr, FIND_OPTIONS);
        const ImageBase* const dst_image = dst_id ? &slot_images[dst_id] : nullptr;
        const ImageBase* const src_image = src_id ? &slot_images[src_id] : nullptr;
        DeduceBlitImages(dst_info, src_info, dst_image, src_image);
        if (GetFormatType(dst_info.format) != GetFormatType(src_info.format)) {
            continue;
        }
        if (!dst_id) {
            dst_id = InsertImage(dst_info, dst_addr, RelaxedOptions{});
        }
        if (!src_id) {
            src_id = InsertImage(src_info, src_addr, RelaxedOptions{});
        }
    } while (has_deleted_images);
    return BlitImages{
        .dst_id = dst_id,
        .src_id = src_id,
        .dst_format = dst_info.format,
        .src_format = src_info.format,
    };
}

template <class P>
SamplerId TextureCache<P>::FindSampler(const TSCEntry& config) {
    if (std::ranges::all_of(config.raw, [](u64 value) { return value == 0; })) {
        return NULL_SAMPLER_ID;
    }
    const auto [pair, is_new] = samplers.try_emplace(config);
    if (is_new) {
        pair->second = slot_samplers.insert(runtime, config);
    }
    return pair->second;
}

template <class P>
ImageViewId TextureCache<P>::FindColorBuffer(size_t index, bool is_clear) {
    const auto& regs = maxwell3d.regs;
    if (index >= regs.rt_control.count) {
        return ImageViewId{};
    }
    const auto& rt = regs.rt[index];
    const GPUVAddr gpu_addr = rt.Address();
    if (gpu_addr == 0) {
        return ImageViewId{};
    }
    if (rt.format == Tegra::RenderTargetFormat::NONE) {
        return ImageViewId{};
    }
    const ImageInfo info(regs, index);
    return FindRenderTargetView(info, gpu_addr, is_clear);
}

template <class P>
ImageViewId TextureCache<P>::FindDepthBuffer(bool is_clear) {
    const auto& regs = maxwell3d.regs;
    if (!regs.zeta_enable) {
        return ImageViewId{};
    }
    const GPUVAddr gpu_addr = regs.zeta.Address();
    if (gpu_addr == 0) {
        return ImageViewId{};
    }
    const ImageInfo info(regs);
    return FindRenderTargetView(info, gpu_addr, is_clear);
}

template <class P>
ImageViewId TextureCache<P>::FindRenderTargetView(const ImageInfo& info, GPUVAddr gpu_addr,
                                                  bool is_clear) {
    const auto options = is_clear ? RelaxedOptions::Samples : RelaxedOptions{};
    const ImageId image_id = FindOrInsertImage(info, gpu_addr, options);
    if (!image_id) {
        return NULL_IMAGE_VIEW_ID;
    }
    Image& image = slot_images[image_id];
    const ImageViewType view_type = RenderTargetImageViewType(info);
    SubresourceBase base;
    if (image.info.type == ImageType::Linear) {
        base = SubresourceBase{.level = 0, .layer = 0};
    } else {
        base = image.TryFindBase(gpu_addr).value();
    }
    const s32 layers = image.info.type == ImageType::e3D ? info.size.depth : info.resources.layers;
    const SubresourceRange range{
        .base = base,
        .extent = {.levels = 1, .layers = layers},
    };
    return FindOrEmplaceImageView(image_id, ImageViewInfo(view_type, info.format, range));
}

template <class P>
template <typename Func>
void TextureCache<P>::ForEachImageInRegion(VAddr cpu_addr, size_t size, Func&& func) {
    using FuncReturn = typename std::invoke_result<Func, ImageId, Image&>::type;
    static constexpr bool BOOL_BREAK = std::is_same_v<FuncReturn, bool>;
    boost::container::small_vector<ImageId, 32> images;
    boost::container::small_vector<ImageMapId, 32> maps;
    ForEachCPUPage(cpu_addr, size, [this, &images, &maps, cpu_addr, size, func](u64 page) {
        const auto it = page_table.find(page);
        if (it == page_table.end()) {
            if constexpr (BOOL_BREAK) {
                return false;
            } else {
                return;
            }
        }
        for (const ImageMapId map_id : it->second) {
            ImageMapView& map = slot_map_views[map_id];
            if (map.picked) {
                continue;
            }
            if (!map.Overlaps(cpu_addr, size)) {
                continue;
            }
            map.picked = true;
            maps.push_back(map_id);
            Image& image = slot_images[map.image_id];
            if (True(image.flags & ImageFlagBits::Picked)) {
                continue;
            }
            image.flags |= ImageFlagBits::Picked;
            images.push_back(map.image_id);
            if constexpr (BOOL_BREAK) {
                if (func(map.image_id, image)) {
                    return true;
                }
            } else {
                func(map.image_id, image);
            }
        }
        if constexpr (BOOL_BREAK) {
            return false;
        }
    });
    for (const ImageId image_id : images) {
        slot_images[image_id].flags &= ~ImageFlagBits::Picked;
    }
    for (const ImageMapId map_id : maps) {
        slot_map_views[map_id].picked = false;
    }
}

template <class P>
template <typename Func>
void TextureCache<P>::ForEachImageInRegionGPU(GPUVAddr gpu_addr, size_t size, Func&& func) {
    using FuncReturn = typename std::invoke_result<Func, ImageId, Image&>::type;
    static constexpr bool BOOL_BREAK = std::is_same_v<FuncReturn, bool>;
    boost::container::small_vector<ImageId, 8> images;
    ForEachGPUPage(gpu_addr, size, [this, &images, gpu_addr, size, func](u64 page) {
        const auto it = gpu_page_table.find(page);
        if (it == gpu_page_table.end()) {
            if constexpr (BOOL_BREAK) {
                return false;
            } else {
                return;
            }
        }
        for (const ImageId image_id : it->second) {
            Image& image = slot_images[image_id];
            if (True(image.flags & ImageFlagBits::Picked)) {
                continue;
            }
            if (!image.OverlapsGPU(gpu_addr, size)) {
                continue;
            }
            image.flags |= ImageFlagBits::Picked;
            images.push_back(image_id);
            if constexpr (BOOL_BREAK) {
                if (func(image_id, image)) {
                    return true;
                }
            } else {
                func(image_id, image);
            }
        }
        if constexpr (BOOL_BREAK) {
            return false;
        }
    });
    for (const ImageId image_id : images) {
        slot_images[image_id].flags &= ~ImageFlagBits::Picked;
    }
}

template <class P>
template <typename Func>
void TextureCache<P>::ForEachSparseImageInRegion(GPUVAddr gpu_addr, size_t size, Func&& func) {
    using FuncReturn = typename std::invoke_result<Func, ImageId, Image&>::type;
    static constexpr bool BOOL_BREAK = std::is_same_v<FuncReturn, bool>;
    boost::container::small_vector<ImageId, 8> images;
    ForEachGPUPage(gpu_addr, size, [this, &images, gpu_addr, size, func](u64 page) {
        const auto it = sparse_page_table.find(page);
        if (it == sparse_page_table.end()) {
            if constexpr (BOOL_BREAK) {
                return false;
            } else {
                return;
            }
        }
        for (const ImageId image_id : it->second) {
            Image& image = slot_images[image_id];
            if (True(image.flags & ImageFlagBits::Picked)) {
                continue;
            }
            if (!image.OverlapsGPU(gpu_addr, size)) {
                continue;
            }
            image.flags |= ImageFlagBits::Picked;
            images.push_back(image_id);
            if constexpr (BOOL_BREAK) {
                if (func(image_id, image)) {
                    return true;
                }
            } else {
                func(image_id, image);
            }
        }
        if constexpr (BOOL_BREAK) {
            return false;
        }
    });
    for (const ImageId image_id : images) {
        slot_images[image_id].flags &= ~ImageFlagBits::Picked;
    }
}

template <class P>
template <typename Func>
void TextureCache<P>::ForEachSparseSegment(ImageBase& image, Func&& func) {
    using FuncReturn = typename std::invoke_result<Func, GPUVAddr, VAddr, size_t>::type;
    static constexpr bool RETURNS_BOOL = std::is_same_v<FuncReturn, bool>;
    const auto segments = gpu_memory.GetSubmappedRange(image.gpu_addr, image.guest_size_bytes);
    for (auto& segment : segments) {
        const auto gpu_addr = segment.first;
        const auto size = segment.second;
        std::optional<VAddr> cpu_addr = gpu_memory.GpuToCpuAddress(gpu_addr);
        ASSERT(cpu_addr);
        if constexpr (RETURNS_BOOL) {
            if (func(gpu_addr, *cpu_addr, size)) {
                break;
            }
        } else {
            func(gpu_addr, *cpu_addr, size);
        }
    }
}

template <class P>
ImageViewId TextureCache<P>::FindOrEmplaceImageView(ImageId image_id, const ImageViewInfo& info) {
    Image& image = slot_images[image_id];
    if (const ImageViewId image_view_id = image.FindView(info); image_view_id) {
        return image_view_id;
    }
    const ImageViewId image_view_id = slot_image_views.insert(runtime, info, image_id, image);
    image.InsertView(info, image_view_id);
    return image_view_id;
}

template <class P>
void TextureCache<P>::RegisterImage(ImageId image_id) {
    ImageBase& image = slot_images[image_id];
    ASSERT_MSG(False(image.flags & ImageFlagBits::Registered),
               "Trying to register an already registered image");
    image.flags |= ImageFlagBits::Registered;
    u64 tentative_size = std::max(image.guest_size_bytes, image.unswizzled_size_bytes);
    if ((IsPixelFormatASTC(image.info.format) &&
         True(image.flags & ImageFlagBits::AcceleratedUpload)) ||
        True(image.flags & ImageFlagBits::Converted)) {
        tentative_size = EstimatedDecompressedSize(tentative_size, image.info.format);
    }
    total_used_memory += Common::AlignUp(tentative_size, 1024);
    ForEachGPUPage(image.gpu_addr, image.guest_size_bytes,
                   [this, image_id](u64 page) { gpu_page_table[page].push_back(image_id); });
    if (False(image.flags & ImageFlagBits::Sparse)) {
        auto map_id =
            slot_map_views.insert(image.gpu_addr, image.cpu_addr, image.guest_size_bytes, image_id);
        ForEachCPUPage(image.cpu_addr, image.guest_size_bytes,
                       [this, map_id](u64 page) { page_table[page].push_back(map_id); });
        image.map_view_id = map_id;
        return;
    }
    std::vector<ImageViewId> sparse_maps{};
    ForEachSparseSegment(
        image, [this, image_id, &sparse_maps](GPUVAddr gpu_addr, VAddr cpu_addr, size_t size) {
            auto map_id = slot_map_views.insert(gpu_addr, cpu_addr, size, image_id);
            ForEachCPUPage(cpu_addr, size,
                           [this, map_id](u64 page) { page_table[page].push_back(map_id); });
            sparse_maps.push_back(map_id);
        });
    sparse_views.emplace(image_id, std::move(sparse_maps));
    ForEachGPUPage(image.gpu_addr, image.guest_size_bytes,
                   [this, image_id](u64 page) { sparse_page_table[page].push_back(image_id); });
}

template <class P>
void TextureCache<P>::UnregisterImage(ImageId image_id) {
    Image& image = slot_images[image_id];
    ASSERT_MSG(True(image.flags & ImageFlagBits::Registered),
               "Trying to unregister an already registered image");
    image.flags &= ~ImageFlagBits::Registered;
    image.flags &= ~ImageFlagBits::BadOverlap;
    u64 tentative_size = std::max(image.guest_size_bytes, image.unswizzled_size_bytes);
    if ((IsPixelFormatASTC(image.info.format) &&
         True(image.flags & ImageFlagBits::AcceleratedUpload)) ||
        True(image.flags & ImageFlagBits::Converted)) {
        tentative_size = EstimatedDecompressedSize(tentative_size, image.info.format);
    }
    total_used_memory -= Common::AlignUp(tentative_size, 1024);
    const auto& clear_page_table =
        [this, image_id](
            u64 page,
            std::unordered_map<u64, std::vector<ImageId>, IdentityHash<u64>>& selected_page_table) {
            const auto page_it = selected_page_table.find(page);
            if (page_it == selected_page_table.end()) {
                UNREACHABLE_MSG("Unregistering unregistered page=0x{:x}", page << PAGE_BITS);
                return;
            }
            std::vector<ImageId>& image_ids = page_it->second;
            const auto vector_it = std::ranges::find(image_ids, image_id);
            if (vector_it == image_ids.end()) {
                UNREACHABLE_MSG("Unregistering unregistered image in page=0x{:x}",
                                page << PAGE_BITS);
                return;
            }
            image_ids.erase(vector_it);
        };
    ForEachGPUPage(image.gpu_addr, image.guest_size_bytes,
                   [this, &clear_page_table](u64 page) { clear_page_table(page, gpu_page_table); });
    if (False(image.flags & ImageFlagBits::Sparse)) {
        const auto map_id = image.map_view_id;
        ForEachCPUPage(image.cpu_addr, image.guest_size_bytes, [this, map_id](u64 page) {
            const auto page_it = page_table.find(page);
            if (page_it == page_table.end()) {
                UNREACHABLE_MSG("Unregistering unregistered page=0x{:x}", page << PAGE_BITS);
                return;
            }
            std::vector<ImageMapId>& image_map_ids = page_it->second;
            const auto vector_it = std::ranges::find(image_map_ids, map_id);
            if (vector_it == image_map_ids.end()) {
                UNREACHABLE_MSG("Unregistering unregistered image in page=0x{:x}",
                                page << PAGE_BITS);
                return;
            }
            image_map_ids.erase(vector_it);
        });
        slot_map_views.erase(map_id);
        return;
    }
    ForEachGPUPage(image.gpu_addr, image.guest_size_bytes, [this, &clear_page_table](u64 page) {
        clear_page_table(page, sparse_page_table);
    });
    auto it = sparse_views.find(image_id);
    ASSERT(it != sparse_views.end());
    auto& sparse_maps = it->second;
    for (auto& map_view_id : sparse_maps) {
        const auto& map_range = slot_map_views[map_view_id];
        const VAddr cpu_addr = map_range.cpu_addr;
        const std::size_t size = map_range.size;
        ForEachCPUPage(cpu_addr, size, [this, image_id](u64 page) {
            const auto page_it = page_table.find(page);
            if (page_it == page_table.end()) {
                UNREACHABLE_MSG("Unregistering unregistered page=0x{:x}", page << PAGE_BITS);
                return;
            }
            std::vector<ImageMapId>& image_map_ids = page_it->second;
            auto vector_it = image_map_ids.begin();
            while (vector_it != image_map_ids.end()) {
                ImageMapView& map = slot_map_views[*vector_it];
                if (map.image_id != image_id) {
                    vector_it++;
                    continue;
                }
                if (!map.picked) {
                    map.picked = true;
                }
                vector_it = image_map_ids.erase(vector_it);
            }
        });
        slot_map_views.erase(map_view_id);
    }
    sparse_views.erase(it);
}

template <class P>
void TextureCache<P>::TrackImage(ImageBase& image, ImageId image_id) {
    ASSERT(False(image.flags & ImageFlagBits::Tracked));
    image.flags |= ImageFlagBits::Tracked;
    if (False(image.flags & ImageFlagBits::Sparse)) {
        rasterizer.UpdatePagesCachedCount(image.cpu_addr, image.guest_size_bytes, 1);
        return;
    }
    if (True(image.flags & ImageFlagBits::Registered)) {
        auto it = sparse_views.find(image_id);
        ASSERT(it != sparse_views.end());
        auto& sparse_maps = it->second;
        for (auto& map_view_id : sparse_maps) {
            const auto& map = slot_map_views[map_view_id];
            const VAddr cpu_addr = map.cpu_addr;
            const std::size_t size = map.size;
            rasterizer.UpdatePagesCachedCount(cpu_addr, size, 1);
        }
        return;
    }
    ForEachSparseSegment(image,
                         [this]([[maybe_unused]] GPUVAddr gpu_addr, VAddr cpu_addr, size_t size) {
                             rasterizer.UpdatePagesCachedCount(cpu_addr, size, 1);
                         });
}

template <class P>
void TextureCache<P>::UntrackImage(ImageBase& image, ImageId image_id) {
    ASSERT(True(image.flags & ImageFlagBits::Tracked));
    image.flags &= ~ImageFlagBits::Tracked;
    if (False(image.flags & ImageFlagBits::Sparse)) {
        rasterizer.UpdatePagesCachedCount(image.cpu_addr, image.guest_size_bytes, -1);
        return;
    }
    ASSERT(True(image.flags & ImageFlagBits::Registered));
    auto it = sparse_views.find(image_id);
    ASSERT(it != sparse_views.end());
    auto& sparse_maps = it->second;
    for (auto& map_view_id : sparse_maps) {
        const auto& map = slot_map_views[map_view_id];
        const VAddr cpu_addr = map.cpu_addr;
        const std::size_t size = map.size;
        rasterizer.UpdatePagesCachedCount(cpu_addr, size, -1);
    }
}

template <class P>
void TextureCache<P>::DeleteImage(ImageId image_id) {
    ImageBase& image = slot_images[image_id];
    const GPUVAddr gpu_addr = image.gpu_addr;
    const auto alloc_it = image_allocs_table.find(gpu_addr);
    if (alloc_it == image_allocs_table.end()) {
        UNREACHABLE_MSG("Trying to delete an image alloc that does not exist in address 0x{:x}",
                        gpu_addr);
        return;
    }
    const ImageAllocId alloc_id = alloc_it->second;
    std::vector<ImageId>& alloc_images = slot_image_allocs[alloc_id].images;
    const auto alloc_image_it = std::ranges::find(alloc_images, image_id);
    if (alloc_image_it == alloc_images.end()) {
        UNREACHABLE_MSG("Trying to delete an image that does not exist");
        return;
    }
    ASSERT_MSG(False(image.flags & ImageFlagBits::Tracked), "Image was not untracked");
    ASSERT_MSG(False(image.flags & ImageFlagBits::Registered), "Image was not unregistered");

    // Mark render targets as dirty
    auto& dirty = maxwell3d.dirty.flags;
    dirty[Dirty::RenderTargets] = true;
    dirty[Dirty::ZetaBuffer] = true;
    for (size_t rt = 0; rt < NUM_RT; ++rt) {
        dirty[Dirty::ColorBuffer0 + rt] = true;
    }
    const std::span<const ImageViewId> image_view_ids = image.image_view_ids;
    for (const ImageViewId image_view_id : image_view_ids) {
        std::ranges::replace(render_targets.color_buffer_ids, image_view_id, ImageViewId{});
        if (render_targets.depth_buffer_id == image_view_id) {
            render_targets.depth_buffer_id = ImageViewId{};
        }
    }
    RemoveImageViewReferences(image_view_ids);
    RemoveFramebuffers(image_view_ids);

    for (const AliasedImage& alias : image.aliased_images) {
        ImageBase& other_image = slot_images[alias.id];
        [[maybe_unused]] const size_t num_removed_aliases =
            std::erase_if(other_image.aliased_images, [image_id](const AliasedImage& other_alias) {
                return other_alias.id == image_id;
            });
        other_image.CheckAliasState();
        ASSERT_MSG(num_removed_aliases == 1, "Invalid number of removed aliases: {}",
                   num_removed_aliases);
    }
    for (const ImageId overlap_id : image.overlapping_images) {
        ImageBase& other_image = slot_images[overlap_id];
        [[maybe_unused]] const size_t num_removed_overlaps = std::erase_if(
            other_image.overlapping_images,
            [image_id](const ImageId other_overlap_id) { return other_overlap_id == image_id; });
        other_image.CheckBadOverlapState();
        ASSERT_MSG(num_removed_overlaps == 1, "Invalid number of removed overlapps: {}",
                   num_removed_overlaps);
    }
    for (const ImageViewId image_view_id : image_view_ids) {
        sentenced_image_view.Push(std::move(slot_image_views[image_view_id]));
        slot_image_views.erase(image_view_id);
    }
    sentenced_images.Push(std::move(slot_images[image_id]));
    slot_images.erase(image_id);

    alloc_images.erase(alloc_image_it);
    if (alloc_images.empty()) {
        image_allocs_table.erase(alloc_it);
    }
    if constexpr (ENABLE_VALIDATION) {
        std::ranges::fill(graphics_image_view_ids, CORRUPT_ID);
        std::ranges::fill(compute_image_view_ids, CORRUPT_ID);
    }
    graphics_image_table.Invalidate();
    compute_image_table.Invalidate();
    has_deleted_images = true;
}

template <class P>
void TextureCache<P>::RemoveImageViewReferences(std::span<const ImageViewId> removed_views) {
    auto it = image_views.begin();
    while (it != image_views.end()) {
        const auto found = std::ranges::find(removed_views, it->second);
        if (found != removed_views.end()) {
            it = image_views.erase(it);
        } else {
            ++it;
        }
    }
}

template <class P>
void TextureCache<P>::RemoveFramebuffers(std::span<const ImageViewId> removed_views) {
    auto it = framebuffers.begin();
    while (it != framebuffers.end()) {
        if (it->first.Contains(removed_views)) {
            it = framebuffers.erase(it);
        } else {
            ++it;
        }
    }
}

template <class P>
void TextureCache<P>::MarkModification(ImageBase& image) noexcept {
    image.flags |= ImageFlagBits::GpuModified;
    image.modification_tick = ++modification_tick;
}

template <class P>
void TextureCache<P>::SynchronizeAliases(ImageId image_id) {
    boost::container::small_vector<const AliasedImage*, 1> aliased_images;
    ImageBase& image = slot_images[image_id];
    u64 most_recent_tick = image.modification_tick;
    for (const AliasedImage& aliased : image.aliased_images) {
        ImageBase& aliased_image = slot_images[aliased.id];
        if (image.modification_tick < aliased_image.modification_tick) {
            most_recent_tick = std::max(most_recent_tick, aliased_image.modification_tick);
            aliased_images.push_back(&aliased);
        }
    }
    if (aliased_images.empty()) {
        return;
    }
    image.modification_tick = most_recent_tick;
    std::ranges::sort(aliased_images, [this](const AliasedImage* lhs, const AliasedImage* rhs) {
        const ImageBase& lhs_image = slot_images[lhs->id];
        const ImageBase& rhs_image = slot_images[rhs->id];
        return lhs_image.modification_tick < rhs_image.modification_tick;
    });
    for (const AliasedImage* const aliased : aliased_images) {
        CopyImage(image_id, aliased->id, aliased->copies);
    }
}

template <class P>
void TextureCache<P>::PrepareImage(ImageId image_id, bool is_modification, bool invalidate) {
    Image& image = slot_images[image_id];
    if (invalidate) {
        image.flags &= ~(ImageFlagBits::CpuModified | ImageFlagBits::GpuModified);
        if (False(image.flags & ImageFlagBits::Tracked)) {
            TrackImage(image, image_id);
        }
    } else {
        RefreshContents(image, image_id);
        SynchronizeAliases(image_id);
    }
    if (is_modification) {
        MarkModification(image);
    }
    image.frame_tick = frame_tick;
}

template <class P>
void TextureCache<P>::PrepareImageView(ImageViewId image_view_id, bool is_modification,
                                       bool invalidate) {
    if (!image_view_id) {
        return;
    }
    const ImageViewBase& image_view = slot_image_views[image_view_id];
    if (image_view.IsBuffer()) {
        return;
    }
    PrepareImage(image_view.image_id, is_modification, invalidate);
}

template <class P>
void TextureCache<P>::CopyImage(ImageId dst_id, ImageId src_id, std::span<const ImageCopy> copies) {
    Image& dst = slot_images[dst_id];
    Image& src = slot_images[src_id];
    const auto dst_format_type = GetFormatType(dst.info.format);
    const auto src_format_type = GetFormatType(src.info.format);
    if (src_format_type == dst_format_type) {
        if constexpr (HAS_EMULATED_COPIES) {
            if (!runtime.CanImageBeCopied(dst, src)) {
                return runtime.EmulateCopyImage(dst, src, copies);
            }
        }
        return runtime.CopyImage(dst, src, copies);
    }
    UNIMPLEMENTED_IF(dst.info.type != ImageType::e2D);
    UNIMPLEMENTED_IF(src.info.type != ImageType::e2D);
    for (const ImageCopy& copy : copies) {
        UNIMPLEMENTED_IF(copy.dst_subresource.num_layers != 1);
        UNIMPLEMENTED_IF(copy.src_subresource.num_layers != 1);
        UNIMPLEMENTED_IF(copy.src_offset != Offset3D{});
        UNIMPLEMENTED_IF(copy.dst_offset != Offset3D{});

        const SubresourceBase dst_base{
            .level = copy.dst_subresource.base_level,
            .layer = copy.dst_subresource.base_layer,
        };
        const SubresourceBase src_base{
            .level = copy.src_subresource.base_level,
            .layer = copy.src_subresource.base_layer,
        };
        const SubresourceExtent dst_extent{.levels = 1, .layers = 1};
        const SubresourceExtent src_extent{.levels = 1, .layers = 1};
        const SubresourceRange dst_range{.base = dst_base, .extent = dst_extent};
        const SubresourceRange src_range{.base = src_base, .extent = src_extent};
        const ImageViewInfo dst_view_info(ImageViewType::e2D, dst.info.format, dst_range);
        const ImageViewInfo src_view_info(ImageViewType::e2D, src.info.format, src_range);
        const auto [dst_framebuffer_id, dst_view_id] = RenderTargetFromImage(dst_id, dst_view_info);
        Framebuffer* const dst_framebuffer = &slot_framebuffers[dst_framebuffer_id];
        const ImageViewId src_view_id = FindOrEmplaceImageView(src_id, src_view_info);
        ImageView& dst_view = slot_image_views[dst_view_id];
        ImageView& src_view = slot_image_views[src_view_id];
        [[maybe_unused]] const Extent3D expected_size{
            .width = std::min(dst_view.size.width, src_view.size.width),
            .height = std::min(dst_view.size.height, src_view.size.height),
            .depth = std::min(dst_view.size.depth, src_view.size.depth),
        };
        UNIMPLEMENTED_IF(copy.extent != expected_size);

        runtime.ConvertImage(dst_framebuffer, dst_view, src_view);
    }
}

template <class P>
void TextureCache<P>::BindRenderTarget(ImageViewId* old_id, ImageViewId new_id) {
    if (*old_id == new_id) {
        return;
    }
    if (*old_id) {
        const ImageViewBase& old_view = slot_image_views[*old_id];
        if (True(old_view.flags & ImageViewFlagBits::PreemtiveDownload)) {
            uncommitted_downloads.push_back(old_view.image_id);
        }
    }
    *old_id = new_id;
}

template <class P>
std::pair<FramebufferId, ImageViewId> TextureCache<P>::RenderTargetFromImage(
    ImageId image_id, const ImageViewInfo& view_info) {
    const ImageViewId view_id = FindOrEmplaceImageView(image_id, view_info);
    const ImageBase& image = slot_images[image_id];
    const bool is_color = GetFormatType(image.info.format) == SurfaceType::ColorTexture;
    const ImageViewId color_view_id = is_color ? view_id : ImageViewId{};
    const ImageViewId depth_view_id = is_color ? ImageViewId{} : view_id;
    const Extent3D extent = MipSize(image.info.size, view_info.range.base.level);
    const u32 num_samples = image.info.num_samples;
    const auto [samples_x, samples_y] = SamplesLog2(num_samples);
    const FramebufferId framebuffer_id = GetFramebufferId(RenderTargets{
        .color_buffer_ids = {color_view_id},
        .depth_buffer_id = depth_view_id,
        .size = {extent.width >> samples_x, extent.height >> samples_y},
    });
    return {framebuffer_id, view_id};
}

template <class P>
bool TextureCache<P>::IsFullClear(ImageViewId id) {
    if (!id) {
        return true;
    }
    const ImageViewBase& image_view = slot_image_views[id];
    const ImageBase& image = slot_images[image_view.image_id];
    const Extent3D size = image_view.size;
    const auto& regs = maxwell3d.regs;
    const auto& scissor = regs.scissor_test[0];
    if (image.info.resources.levels > 1 || image.info.resources.layers > 1) {
        // Images with multiple resources can't be cleared in a single call
        return false;
    }
    if (regs.clear_flags.scissor == 0) {
        // If scissor testing is disabled, the clear is always full
        return true;
    }
    // Make sure the clear covers all texels in the subresource
    return scissor.min_x == 0 && scissor.min_y == 0 && scissor.max_x >= size.width &&
           scissor.max_y >= size.height;
}

} // namespace VideoCommon
