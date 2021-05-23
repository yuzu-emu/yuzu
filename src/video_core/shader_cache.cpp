// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <vector>

#include "common/assert.h"
#include "shader_recompiler/frontend/maxwell/control_flow.h"
#include "shader_recompiler/object_pool.h"
#include "video_core/dirty_flags.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/memory_manager.h"
#include "video_core/shader_cache.h"
#include "video_core/shader_environment.h"

namespace VideoCommon {

void ShaderCache::InvalidateRegion(VAddr addr, size_t size) {
    std::scoped_lock lock{invalidation_mutex};
    InvalidatePagesInRegion(addr, size);
    RemovePendingShaders();
}

void ShaderCache::OnCPUWrite(VAddr addr, size_t size) {
    std::lock_guard lock{invalidation_mutex};
    InvalidatePagesInRegion(addr, size);
}

void ShaderCache::SyncGuestHost() {
    std::scoped_lock lock{invalidation_mutex};
    RemovePendingShaders();
}

ShaderCache::ShaderCache(VideoCore::RasterizerInterface& rasterizer_,
                         Tegra::MemoryManager& gpu_memory_, Tegra::Engines::Maxwell3D& maxwell3d_,
                         Tegra::Engines::KeplerCompute& kepler_compute_)
    : gpu_memory{gpu_memory_}, maxwell3d{maxwell3d_}, kepler_compute{kepler_compute_},
      rasterizer{rasterizer_} {}

bool ShaderCache::RefreshStages(std::array<u64, 6>& unique_hashes) {
    auto& dirty{maxwell3d.dirty.flags};
    if (!dirty[VideoCommon::Dirty::Shaders]) {
        return last_shaders_valid;
    }
    dirty[VideoCommon::Dirty::Shaders] = false;

    const GPUVAddr base_addr{maxwell3d.regs.code_address.CodeAddress()};
    for (size_t index = 0; index < Tegra::Engines::Maxwell3D::Regs::MaxShaderProgram; ++index) {
        if (!maxwell3d.regs.IsShaderConfigEnabled(index)) {
            unique_hashes[index] = 0;
            continue;
        }
        const auto& shader_config{maxwell3d.regs.shader_config[index]};
        const auto program{static_cast<Tegra::Engines::Maxwell3D::Regs::ShaderProgram>(index)};
        const GPUVAddr shader_addr{base_addr + shader_config.offset};
        const std::optional<VAddr> cpu_shader_addr{gpu_memory.GpuToCpuAddress(shader_addr)};
        if (!cpu_shader_addr) {
            LOG_ERROR(HW_GPU, "Invalid GPU address for shader 0x{:016x}", shader_addr);
            last_shaders_valid = false;
            return false;
        }
        const ShaderInfo* shader_info{TryGet(*cpu_shader_addr)};
        if (!shader_info) {
            const u32 start_address{shader_config.offset};
            GraphicsEnvironment env{maxwell3d, gpu_memory, program, base_addr, start_address};
            shader_info = MakeShaderInfo(env, *cpu_shader_addr);
        }
        shader_infos[index] = shader_info;
        unique_hashes[index] = shader_info->unique_hash;
    }
    last_shaders_valid = true;
    return true;
}

const ShaderInfo* ShaderCache::ComputeShader() {
    const GPUVAddr program_base{kepler_compute.regs.code_loc.Address()};
    const auto& qmd{kepler_compute.launch_description};
    const GPUVAddr shader_addr{program_base + qmd.program_start};
    const std::optional<VAddr> cpu_shader_addr{gpu_memory.GpuToCpuAddress(shader_addr)};
    if (!cpu_shader_addr) {
        LOG_ERROR(HW_GPU, "Invalid GPU address for shader 0x{:016x}", shader_addr);
        return nullptr;
    }
    if (const ShaderInfo* const shader = TryGet(*cpu_shader_addr)) {
        return shader;
    }
    ComputeEnvironment env{kepler_compute, gpu_memory, program_base, qmd.program_start};
    return MakeShaderInfo(env, *cpu_shader_addr);
}

void ShaderCache::GetGraphicsEnvironments(GraphicsEnvironments& result,
                                          const std::array<u64, NUM_PROGRAMS>& unique_hashes) {
    size_t env_index{};
    const GPUVAddr base_addr{maxwell3d.regs.code_address.CodeAddress()};
    for (size_t index = 0; index < NUM_PROGRAMS; ++index) {
        if (unique_hashes[index] == 0) {
            continue;
        }
        const auto program{static_cast<Tegra::Engines::Maxwell3D::Regs::ShaderProgram>(index)};
        auto& env{result.envs[index]};
        const u32 start_address{maxwell3d.regs.shader_config[index].offset};
        env = GraphicsEnvironment{maxwell3d, gpu_memory, program, base_addr, start_address};
        env.SetCachedSize(shader_infos[index]->size_bytes);
        result.env_ptrs[env_index++] = &env;
    }
}

ShaderInfo* ShaderCache::TryGet(VAddr addr) const {
    std::scoped_lock lock{lookup_mutex};

    const auto it = lookup_cache.find(addr);
    if (it == lookup_cache.end()) {
        return nullptr;
    }
    return it->second->data;
}

void ShaderCache::Register(std::unique_ptr<ShaderInfo> data, VAddr addr, size_t size) {
    std::scoped_lock lock{invalidation_mutex, lookup_mutex};

    const VAddr addr_end = addr + size;
    Entry* const entry = NewEntry(addr, addr_end, data.get());

    const u64 page_end = (addr_end + PAGE_SIZE - 1) >> PAGE_BITS;
    for (u64 page = addr >> PAGE_BITS; page < page_end; ++page) {
        invalidation_cache[page].push_back(entry);
    }

    storage.push_back(std::move(data));

    rasterizer.UpdatePagesCachedCount(addr, size, 1);
}

void ShaderCache::InvalidatePagesInRegion(VAddr addr, size_t size) {
    const VAddr addr_end = addr + size;
    const u64 page_end = (addr_end + PAGE_SIZE - 1) >> PAGE_BITS;
    for (u64 page = addr >> PAGE_BITS; page < page_end; ++page) {
        auto it = invalidation_cache.find(page);
        if (it == invalidation_cache.end()) {
            continue;
        }
        InvalidatePageEntries(it->second, addr, addr_end);
    }
}

void ShaderCache::RemovePendingShaders() {
    if (marked_for_removal.empty()) {
        return;
    }
    // Remove duplicates
    std::ranges::sort(marked_for_removal);
    marked_for_removal.erase(std::unique(marked_for_removal.begin(), marked_for_removal.end()),
                             marked_for_removal.end());

    std::vector<ShaderInfo*> removed_shaders;
    removed_shaders.reserve(marked_for_removal.size());

    std::scoped_lock lock{lookup_mutex};

    for (Entry* const entry : marked_for_removal) {
        removed_shaders.push_back(entry->data);

        const auto it = lookup_cache.find(entry->addr_start);
        ASSERT(it != lookup_cache.end());
        lookup_cache.erase(it);
    }
    marked_for_removal.clear();

    if (!removed_shaders.empty()) {
        RemoveShadersFromStorage(std::move(removed_shaders));
    }
}

void ShaderCache::InvalidatePageEntries(std::vector<Entry*>& entries, VAddr addr, VAddr addr_end) {
    size_t index = 0;
    while (index < entries.size()) {
        Entry* const entry = entries[index];
        if (!entry->Overlaps(addr, addr_end)) {
            ++index;
            continue;
        }

        UnmarkMemory(entry);
        RemoveEntryFromInvalidationCache(entry);
        marked_for_removal.push_back(entry);
    }
}

void ShaderCache::RemoveEntryFromInvalidationCache(const Entry* entry) {
    const u64 page_end = (entry->addr_end + PAGE_SIZE - 1) >> PAGE_BITS;
    for (u64 page = entry->addr_start >> PAGE_BITS; page < page_end; ++page) {
        const auto entries_it = invalidation_cache.find(page);
        ASSERT(entries_it != invalidation_cache.end());
        std::vector<Entry*>& entries = entries_it->second;

        const auto entry_it = std::ranges::find(entries, entry);
        ASSERT(entry_it != entries.end());
        entries.erase(entry_it);
    }
}

void ShaderCache::UnmarkMemory(Entry* entry) {
    if (!entry->is_memory_marked) {
        return;
    }
    entry->is_memory_marked = false;

    const VAddr addr = entry->addr_start;
    const size_t size = entry->addr_end - addr;
    rasterizer.UpdatePagesCachedCount(addr, size, -1);
}

void ShaderCache::RemoveShadersFromStorage(std::vector<ShaderInfo*> removed_shaders) {
    // Remove them from the cache
    std::erase_if(storage, [&removed_shaders](const std::unique_ptr<ShaderInfo>& shader) {
        return std::ranges::find(removed_shaders, shader.get()) != removed_shaders.end();
    });
}

ShaderCache::Entry* ShaderCache::NewEntry(VAddr addr, VAddr addr_end, ShaderInfo* data) {
    auto entry = std::make_unique<Entry>(Entry{addr, addr_end, data});
    Entry* const entry_pointer = entry.get();

    lookup_cache.emplace(addr, std::move(entry));
    return entry_pointer;
}

const ShaderInfo* ShaderCache::MakeShaderInfo(GenericEnvironment& env, VAddr cpu_addr) {
    auto info = std::make_unique<ShaderInfo>();
    if (const std::optional<u64> cached_hash{env.Analyze()}) {
        info->unique_hash = *cached_hash;
        info->size_bytes = env.CachedSize();
    } else {
        // Slow path, not really hit on commercial games
        // Build a control flow graph to get the real shader size
        Shader::ObjectPool<Shader::Maxwell::Flow::Block> flow_block;
        Shader::Maxwell::Flow::CFG cfg{env, flow_block, env.StartAddress()};
        info->unique_hash = env.CalculateHash();
        info->size_bytes = env.ReadSize();
    }
    const size_t size_bytes{info->size_bytes};
    const ShaderInfo* const result{info.get()};
    Register(std::move(info), cpu_addr, size_bytes);
    return result;
}

} // namespace VideoCommon
