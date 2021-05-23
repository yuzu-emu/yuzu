// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <array>
#include <memory>
#include <mutex>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common/common_types.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/shader_environment.h"

namespace Tegra {
class MemoryManager;
}

namespace VideoCommon {

class GenericEnvironment;

struct ShaderInfo {
    u64 unique_hash{};
    size_t size_bytes{};
};

class ShaderCache {
    static constexpr u64 PAGE_BITS = 14;
    static constexpr u64 PAGE_SIZE = u64(1) << PAGE_BITS;

    static constexpr size_t NUM_PROGRAMS = 6;

    struct Entry {
        VAddr addr_start;
        VAddr addr_end;
        ShaderInfo* data;

        bool is_memory_marked = true;

        bool Overlaps(VAddr start, VAddr end) const noexcept {
            return start < addr_end && addr_start < end;
        }
    };

public:
    /// @brief Removes shaders inside a given region
    /// @note Checks for ranges
    /// @param addr Start address of the invalidation
    /// @param size Number of bytes of the invalidation
    void InvalidateRegion(VAddr addr, size_t size);

    /// @brief Unmarks a memory region as cached and marks it for removal
    /// @param addr Start address of the CPU write operation
    /// @param size Number of bytes of the CPU write operation
    void OnCPUWrite(VAddr addr, size_t size);

    /// @brief Flushes delayed removal operations
    void SyncGuestHost();

protected:
    struct GraphicsEnvironments {
        std::array<GraphicsEnvironment, NUM_PROGRAMS> envs;
        std::array<Shader::Environment*, NUM_PROGRAMS> env_ptrs;

        std::span<Shader::Environment* const> Span() const noexcept {
            return std::span(env_ptrs.begin(), std::ranges::find(env_ptrs, nullptr));
        }
    };

    explicit ShaderCache(VideoCore::RasterizerInterface& rasterizer_,
                         Tegra::MemoryManager& gpu_memory_, Tegra::Engines::Maxwell3D& maxwell3d_,
                         Tegra::Engines::KeplerCompute& kepler_compute_);

    /// @brief Update the hashes and information of shader stages
    /// @param unique_hashes Shader hashes to store into when a stage is enabled
    /// @return True no success, false on error
    bool RefreshStages(std::array<u64, NUM_PROGRAMS>& unique_hashes);

    /// @brief Returns information about the current compute shader
    /// @return Pointer to a valid shader, nullptr on error
    const ShaderInfo* ComputeShader();

    /// @brief Collect the current graphics environments
    void GetGraphicsEnvironments(GraphicsEnvironments& result,
                                 const std::array<u64, NUM_PROGRAMS>& unique_hashes);

    Tegra::MemoryManager& gpu_memory;
    Tegra::Engines::Maxwell3D& maxwell3d;
    Tegra::Engines::KeplerCompute& kepler_compute;

    std::array<const ShaderInfo*, NUM_PROGRAMS> shader_infos{};
    bool last_shaders_valid = false;

private:
    /// @brief Tries to obtain a cached shader starting in a given address
    /// @note Doesn't check for ranges, the given address has to be the start of the shader
    /// @param addr Start address of the shader, this doesn't cache for region
    /// @return Pointer to a valid shader, nullptr when nothing is found
    ShaderInfo* TryGet(VAddr addr) const;

    /// @brief Register in the cache a given entry
    /// @param data Shader to store in the cache
    /// @param addr Start address of the shader that will be registered
    /// @param size Size in bytes of the shader
    void Register(std::unique_ptr<ShaderInfo> data, VAddr addr, size_t size);

    /// @brief Invalidate pages in a given region
    /// @pre invalidation_mutex is locked
    void InvalidatePagesInRegion(VAddr addr, size_t size);

    /// @brief Remove shaders marked for deletion
    /// @pre invalidation_mutex is locked
    void RemovePendingShaders();

    /// @brief Invalidates entries in a given range for the passed page
    /// @param entries         Vector of entries in the page, it will be modified on overlaps
    /// @param addr            Start address of the invalidation
    /// @param addr_end        Non-inclusive end address of the invalidation
    /// @pre invalidation_mutex is locked
    void InvalidatePageEntries(std::vector<Entry*>& entries, VAddr addr, VAddr addr_end);

    /// @brief Removes all references to an entry in the invalidation cache
    /// @param entry Entry to remove from the invalidation cache
    /// @pre invalidation_mutex is locked
    void RemoveEntryFromInvalidationCache(const Entry* entry);

    /// @brief Unmarks an entry from the rasterizer cache
    /// @param entry Entry to unmark from memory
    void UnmarkMemory(Entry* entry);

    /// @brief Removes a vector of shaders from a list
    /// @param removed_shaders Shaders to be removed from the storage
    /// @pre invalidation_mutex is locked
    /// @pre lookup_mutex is locked
    void RemoveShadersFromStorage(std::vector<ShaderInfo*> removed_shaders);

    /// @brief Creates a new entry in the lookup cache and returns its pointer
    /// @pre lookup_mutex is locked
    Entry* NewEntry(VAddr addr, VAddr addr_end, ShaderInfo* data);

    /// @brief Create a new shader entry and register it
    const ShaderInfo* MakeShaderInfo(GenericEnvironment& env, VAddr cpu_addr);

    VideoCore::RasterizerInterface& rasterizer;

    mutable std::mutex lookup_mutex;
    std::mutex invalidation_mutex;

    std::unordered_map<u64, std::unique_ptr<Entry>> lookup_cache;
    std::unordered_map<u64, std::vector<Entry*>> invalidation_cache;
    std::vector<std::unique_ptr<ShaderInfo>> storage;
    std::vector<Entry*> marked_for_removal;
};

} // namespace VideoCommon
