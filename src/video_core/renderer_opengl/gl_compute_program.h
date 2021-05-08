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

class ProgramManager;

struct ComputeProgramKey {
    u64 unique_hash;
    u32 shared_memory_size;
    std::array<u32, 3> workgroup_size;

    size_t Hash() const noexcept;

    bool operator==(const ComputeProgramKey&) const noexcept;

    bool operator!=(const ComputeProgramKey& rhs) const noexcept {
        return !operator==(rhs);
    }
};
static_assert(std::has_unique_object_representations_v<ComputeProgramKey>);
static_assert(std::is_trivially_copyable_v<ComputeProgramKey>);
static_assert(std::is_trivially_constructible_v<ComputeProgramKey>);

class ComputeProgram {
public:
    explicit ComputeProgram(TextureCache& texture_cache_, BufferCache& buffer_cache_,
                            Tegra::MemoryManager& gpu_memory_,
                            Tegra::Engines::KeplerCompute& kepler_compute_,
                            ProgramManager& program_manager_, const Shader::Info& info_,
                            OGLProgram source_program_, OGLAssemblyProgram assembly_program_);

    void Configure();

private:
    TextureCache& texture_cache;
    BufferCache& buffer_cache;
    Tegra::MemoryManager& gpu_memory;
    Tegra::Engines::KeplerCompute& kepler_compute;
    ProgramManager& program_manager;

    Shader::Info info;
    OGLProgram source_program;
    OGLAssemblyProgram assembly_program;

    u32 num_texture_buffers{};
    u32 num_image_buffers{};
};

} // namespace OpenGL

namespace std {
template <>
struct hash<OpenGL::ComputeProgramKey> {
    size_t operator()(const OpenGL::ComputeProgramKey& k) const noexcept {
        return k.Hash();
    }
};
} // namespace std
