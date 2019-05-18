// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "video_core/gpu.h"
#include "video_core/morton.h"
#include "video_core/texture_cache/copy_params.h"
#include "video_core/texture_cache/surface_params.h"
#include "video_core/texture_cache/surface_view.h"

namespace Tegra {
class MemoryManager;
}

namespace VideoCommon {

using VideoCore::MortonSwizzleMode;
using VideoCore::Surface::SurfaceTarget;

enum class MatchStructureResult : u32 {
    FullMatch = 0,
    SemiMatch = 1,
    None = 2,
};

class SurfaceBaseImpl {
public:
    void LoadBuffer(Tegra::MemoryManager& memory_manager, std::vector<u8>& staging_buffer);

    void FlushBuffer(Tegra::MemoryManager& memory_manager, std::vector<u8>& staging_buffer);

    GPUVAddr GetGpuAddr() const {
        return gpu_addr;
    }

    bool Overlaps(const CacheAddr start, const CacheAddr end) const {
        return (cache_addr < end) && (cache_addr_end > start);
    }

    bool IsInside(const GPUVAddr other_start, const GPUVAddr other_end) {
        const GPUVAddr gpu_addr_end = gpu_addr + guest_memory_size;
        return (gpu_addr <= other_start && other_end <= gpu_addr_end);
    }

    // Use only when recycling a surface
    void SetGpuAddr(const GPUVAddr new_addr) {
        gpu_addr = new_addr;
    }

    VAddr GetCpuAddr() const {
        return cpu_addr;
    }

    void SetCpuAddr(const VAddr new_addr) {
        cpu_addr = new_addr;
    }

    CacheAddr GetCacheAddr() const {
        return cache_addr;
    }

    CacheAddr GetCacheAddrEnd() const {
        return cache_addr_end;
    }

    void SetCacheAddr(const CacheAddr new_addr) {
        cache_addr = new_addr;
        cache_addr_end = new_addr + guest_memory_size;
    }

    const SurfaceParams& GetSurfaceParams() const {
        return params;
    }

    std::size_t GetSizeInBytes() const {
        return guest_memory_size;
    }

    std::size_t GetHostSizeInBytes() const {
        return host_memory_size;
    }

    std::size_t GetMipmapSize(const u32 level) const {
        return mipmap_sizes[level];
    }

    bool IsLinear() const {
        return !params.is_tiled;
    }

    bool MatchFormat(VideoCore::Surface::PixelFormat pixel_format) const {
        return params.pixel_format == pixel_format;
    }

    bool MatchTarget(VideoCore::Surface::SurfaceTarget target) const {
        return params.target == target;
    }

    bool MatchesSubTexture(const SurfaceParams& rhs, const GPUVAddr other_gpu_addr) const {
        return std::tie(gpu_addr, params.target, params.num_levels) ==
                   std::tie(other_gpu_addr, rhs.target, rhs.num_levels) &&
               params.target == SurfaceTarget::Texture2D && params.num_levels == 1;
    }

    bool MatchesTopology(const SurfaceParams& rhs) const {
        const u32 src_bpp{params.GetBytesPerPixel()};
        const u32 dst_bpp{rhs.GetBytesPerPixel()};
        return std::tie(src_bpp, params.is_tiled) == std::tie(dst_bpp, rhs.is_tiled);
    }

    MatchStructureResult MatchesStructure(const SurfaceParams& rhs) const {
        if (!params.is_tiled) {
            if (std::tie(params.width, params.height, params.pitch) ==
                std::tie(rhs.width, rhs.height, rhs.pitch)) {
                return MatchStructureResult::FullMatch;
            }
            return MatchStructureResult::None;
        }
        // Tiled surface
        if (std::tie(params.depth, params.block_width, params.block_height, params.block_depth,
                     params.tile_width_spacing, params.num_levels) ==
            std::tie(rhs.depth, rhs.block_width, rhs.block_height, rhs.block_depth,
                     rhs.tile_width_spacing, rhs.num_levels)) {
            if (std::tie(params.width, params.height) == std::tie(rhs.width, rhs.height)) {
                return MatchStructureResult::FullMatch;
            }
            const u32 ws = SurfaceParams::ConvertWidth(rhs.GetBlockAlignedWidth(),
                                                       params.pixel_format, rhs.pixel_format);
            const u32 hs =
                SurfaceParams::ConvertHeight(rhs.height, params.pixel_format, rhs.pixel_format);
            const u32 w1 = params.GetBlockAlignedWidth();
            if (std::tie(w1, params.height) == std::tie(ws, hs)) {
                return MatchStructureResult::SemiMatch;
            }
        }
        return MatchStructureResult::None;
    }

    std::optional<std::pair<u32, u32>> GetLayerMipmap(const GPUVAddr candidate_gpu_addr) const {
        if (candidate_gpu_addr < gpu_addr) {
            return {};
        }
        const auto relative_address{static_cast<GPUVAddr>(candidate_gpu_addr - gpu_addr)};
        const auto layer{static_cast<u32>(relative_address / layer_size)};
        const GPUVAddr mipmap_address = relative_address - layer_size * layer;
        const auto mipmap_it =
            Common::BinaryFind(mipmap_offsets.begin(), mipmap_offsets.end(), mipmap_address);
        if (mipmap_it == mipmap_offsets.end()) {
            return {};
        }
        const auto level{static_cast<u32>(std::distance(mipmap_offsets.begin(), mipmap_it))};
        return std::make_pair(layer, level);
    }

    std::vector<CopyParams> BreakDown(const SurfaceParams& in_params) const {
        return params.is_layered ? BreakDownLayered(in_params) : BreakDownNonLayered(in_params);
    }

protected:
    explicit SurfaceBaseImpl(GPUVAddr gpu_addr, const SurfaceParams& params);
    ~SurfaceBaseImpl() = default;

    virtual void DecorateSurfaceName() = 0;

    const SurfaceParams params;
    std::size_t layer_size;
    std::size_t guest_memory_size;
    const std::size_t host_memory_size;
    GPUVAddr gpu_addr{};
    CacheAddr cache_addr{};
    CacheAddr cache_addr_end{};
    VAddr cpu_addr{};

    std::vector<std::size_t> mipmap_sizes;
    std::vector<std::size_t> mipmap_offsets;

private:
    void SwizzleFunc(MortonSwizzleMode mode, u8* memory, const SurfaceParams& params, u8* buffer,
                     u32 level);

    std::vector<CopyParams> BreakDownLayered(const SurfaceParams& in_params) const {
        const u32 layers{params.depth};
        const u32 mipmaps{params.num_levels};
        std::vector<CopyParams> result;
        result.reserve(static_cast<std::size_t>(layers) * static_cast<std::size_t>(mipmaps));

        for (u32 layer = 0; layer < layers; layer++) {
            for (u32 level = 0; level < mipmaps; level++) {
                const u32 width{std::min(params.GetMipWidth(level), in_params.GetMipWidth(level))};
                const u32 height{
                    std::min(params.GetMipHeight(level), in_params.GetMipHeight(level))};
                result.emplace_back(width, height, layer, level);
            }
        }
        return result;
    }

    std::vector<CopyParams> BreakDownNonLayered(const SurfaceParams& in_params) const {
        const u32 mipmaps{params.num_levels};
        std::vector<CopyParams> result;
        result.reserve(mipmaps);

        for (u32 level = 0; level < mipmaps; level++) {
            const u32 width{std::min(params.GetMipWidth(level), in_params.GetMipWidth(level))};
            const u32 height{std::min(params.GetMipHeight(level), in_params.GetMipHeight(level))};
            const u32 depth{std::min(params.GetMipDepth(level), in_params.GetMipDepth(level))};
            result.emplace_back(width, height, depth, level);
        }
        return result;
    }
};

template <typename TView>
class SurfaceBase : public SurfaceBaseImpl {
public:
    virtual void UploadTexture(std::vector<u8>& staging_buffer) = 0;

    virtual void DownloadTexture(std::vector<u8>& staging_buffer) = 0;

    void MarkAsModified(const bool is_modified_, const u64 tick) {
        is_modified = is_modified_ || is_target;
        modification_tick = tick;
    }

    void MarkAsRenderTarget(const bool is_target) {
        this->is_target = is_target;
    }

    void MarkAsPicked(const bool is_picked) {
        this->is_picked = is_picked;
    }

    bool IsModified() const {
        return is_modified;
    }

    bool IsProtected() const {
        // Only 3D Slices are to be protected
        return is_target && params.block_depth > 0;
    }

    bool IsRenderTarget() const {
        return is_target;
    }

    bool IsRegistered() const {
        return is_registered;
    }

    bool IsPicked() const {
        return is_picked;
    }

    void MarkAsRegistered(bool is_reg) {
        is_registered = is_reg;
    }

    u64 GetModificationTick() const {
        return modification_tick;
    }

    TView EmplaceOverview(const SurfaceParams& overview_params) {
        const u32 num_layers{(params.is_layered && !overview_params.is_layered) ? 1 : params.depth};
        return GetView(ViewParams(overview_params.target, 0, num_layers, 0, params.num_levels));
    }

    std::optional<TView> EmplaceView(const SurfaceParams& view_params, const GPUVAddr view_addr,
                                     const std::size_t candidate_size) {
        if (params.target == SurfaceTarget::Texture3D ||
            (params.num_levels == 1 && !params.is_layered) ||
            view_params.target == SurfaceTarget::Texture3D) {
            return {};
        }
        const auto layer_mipmap{GetLayerMipmap(view_addr)};
        if (!layer_mipmap) {
            return {};
        }
        const u32 layer{layer_mipmap->first};
        const u32 mipmap{layer_mipmap->second};
        if (GetMipmapSize(mipmap) != candidate_size) {
            // TODO: The view may cover many mimaps, this case can still go on.
            // This edge-case can be safely be ignored since it will just result in worse
            // performance.
            return {};
        }
        return GetView(ViewParams(view_params.target, layer, 1, mipmap, 1));
    }

    TView GetMainView() const {
        return main_view;
    }

protected:
    explicit SurfaceBase(const GPUVAddr gpu_addr, const SurfaceParams& params)
        : SurfaceBaseImpl(gpu_addr, params) {}

    ~SurfaceBase() = default;

    virtual TView CreateView(const ViewParams& view_key) = 0;

    std::unordered_map<ViewParams, TView> views;
    TView main_view;

private:
    TView GetView(const ViewParams& key) {
        const auto [entry, is_cache_miss] = views.try_emplace(key);
        auto& view{entry->second};
        if (is_cache_miss) {
            view = CreateView(key);
        }
        return view;
    }

    bool is_modified{};
    bool is_target{};
    bool is_registered{};
    bool is_picked{};
    u64 modification_tick{};
};

} // namespace VideoCommon
