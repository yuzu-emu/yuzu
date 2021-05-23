// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <type_traits>
#include <utility>

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/shader_info.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_opengl/gl_buffer_cache.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_texture_cache.h"

namespace OpenGL {

class ProgramManager;

using Maxwell = Tegra::Engines::Maxwell3D::Regs;

struct GraphicsProgramKey {
    struct TransformFeedbackState {
        struct Layout {
            u32 stream;
            u32 varying_count;
            u32 stride;
        };
        std::array<Layout, Maxwell::NumTransformFeedbackBuffers> layouts;
        std::array<std::array<u8, 128>, Maxwell::NumTransformFeedbackBuffers> varyings;
    };

    std::array<u64, 6> unique_hashes;
    union {
        u32 raw;
        BitField<0, 1, u32> xfb_enabled;
        BitField<1, 1, u32> early_z;
        BitField<2, 4, Maxwell::PrimitiveTopology> gs_input_topology;
        BitField<6, 2, Maxwell::TessellationPrimitive> tessellation_primitive;
        BitField<8, 2, Maxwell::TessellationSpacing> tessellation_spacing;
        BitField<10, 1, u32> tessellation_clockwise;
    };
    std::array<u32, 3> padding;
    TransformFeedbackState xfb_state;

    size_t Hash() const noexcept;

    bool operator==(const GraphicsProgramKey&) const noexcept;

    bool operator!=(const GraphicsProgramKey& rhs) const noexcept {
        return !operator==(rhs);
    }

    [[nodiscard]] size_t Size() const noexcept {
        if (xfb_enabled != 0) {
            return sizeof(GraphicsProgramKey);
        } else {
            return offsetof(GraphicsProgramKey, padding);
        }
    }
};
static_assert(std::has_unique_object_representations_v<GraphicsProgramKey>);
static_assert(std::is_trivially_copyable_v<GraphicsProgramKey>);
static_assert(std::is_trivially_constructible_v<GraphicsProgramKey>);

class GraphicsProgram {
public:
    explicit GraphicsProgram(TextureCache& texture_cache_, BufferCache& buffer_cache_,
                             Tegra::MemoryManager& gpu_memory_,
                             Tegra::Engines::Maxwell3D& maxwell3d_,
                             ProgramManager& program_manager_, StateTracker& state_tracker_,
                             OGLProgram program_, const std::array<const Shader::Info*, 5>& infos);

    void Configure(bool is_indexed);

private:
    TextureCache& texture_cache;
    BufferCache& buffer_cache;
    Tegra::MemoryManager& gpu_memory;
    Tegra::Engines::Maxwell3D& maxwell3d;
    ProgramManager& program_manager;
    StateTracker& state_tracker;

    OGLProgram program;
    std::array<Shader::Info, 5> stage_infos{};
    std::array<u32, 5> base_uniform_bindings{};
    std::array<u32, 5> base_storage_bindings{};
    std::array<u32, 5> num_texture_buffers{};
    std::array<u32, 5> num_image_buffers{};
};

} // namespace OpenGL

namespace std {
template <>
struct hash<OpenGL::GraphicsProgramKey> {
    size_t operator()(const OpenGL::GraphicsProgramKey& k) const noexcept {
        return k.Hash();
    }
};
} // namespace std
