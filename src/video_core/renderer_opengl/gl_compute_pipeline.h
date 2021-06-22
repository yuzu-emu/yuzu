// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <type_traits>
#include <utility>

#include "common/common_types.h"
#include "shader_recompiler/shader_info.h"
#include "video_core/renderer_opengl/gl_buffer_cache.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_texture_cache.h"

namespace Tegra {
class MemoryManager;
}

namespace Tegra::Engines {
class KeplerCompute;
}

namespace Shader {
struct Info;
}

namespace OpenGL {

class Device;
class ProgramManager;

struct ComputePipelineKey {
    u64 unique_hash;
    u32 shared_memory_size;
    std::array<u32, 3> workgroup_size;

    size_t Hash() const noexcept;

    bool operator==(const ComputePipelineKey&) const noexcept;

    bool operator!=(const ComputePipelineKey& rhs) const noexcept {
        return !operator==(rhs);
    }
};
static_assert(std::has_unique_object_representations_v<ComputePipelineKey>);
static_assert(std::is_trivially_copyable_v<ComputePipelineKey>);
static_assert(std::is_trivially_constructible_v<ComputePipelineKey>);

class ComputePipeline {
public:
    explicit ComputePipeline(const Device& device, TextureCache& texture_cache_,
                             BufferCache& buffer_cache_, Tegra::MemoryManager& gpu_memory_,
                             Tegra::Engines::KeplerCompute& kepler_compute_,
                             ProgramManager& program_manager_, const Shader::Info& info_,
                             std::string code, std::vector<u32> code_v);

    void Configure();

    [[nodiscard]] bool WritesGlobalMemory() const noexcept {
        return writes_global_memory;
    }

private:
    TextureCache& texture_cache;
    BufferCache& buffer_cache;
    Tegra::MemoryManager& gpu_memory;
    Tegra::Engines::KeplerCompute& kepler_compute;
    ProgramManager& program_manager;

    Shader::Info info;
    OGLProgram source_program;
    OGLAssemblyProgram assembly_program;
    VideoCommon::ComputeUniformBufferSizes uniform_buffer_sizes{};

    u32 num_texture_buffers{};
    u32 num_image_buffers{};

    bool use_storage_buffers{};
    bool writes_global_memory{};
};

} // namespace OpenGL

namespace std {
template <>
struct hash<OpenGL::ComputePipelineKey> {
    size_t operator()(const OpenGL::ComputePipelineKey& k) const noexcept {
        return k.Hash();
    }
};
} // namespace std
