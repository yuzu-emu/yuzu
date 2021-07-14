// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstring>
#include <type_traits>
#include <utility>

#include "common/bit_field.h"
#include "common/cityhash.h"
#include "common/common_types.h"
#include "shader_recompiler/shader_info.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_opengl/gl_buffer_cache.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_texture_cache.h"
#include "video_core/transform_feedback.h"

namespace OpenGL {

namespace ShaderContext {
struct Context;
}

class Device;
class ProgramManager;

using Maxwell = Tegra::Engines::Maxwell3D::Regs;
using ShaderWorker = Common::StatefulThreadWorker<ShaderContext::Context>;

struct GraphicsPipelineKey {
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
    VideoCommon::TransformFeedbackState xfb_state;

    size_t Hash() const noexcept {
        return static_cast<size_t>(Common::CityHash64(reinterpret_cast<const char*>(this), Size()));
    }

    bool operator==(const GraphicsPipelineKey& rhs) const noexcept {
        return std::memcmp(this, &rhs, Size()) == 0;
    }

    bool operator!=(const GraphicsPipelineKey& rhs) const noexcept {
        return !operator==(rhs);
    }

    [[nodiscard]] size_t Size() const noexcept {
        if (xfb_enabled != 0) {
            return sizeof(GraphicsPipelineKey);
        } else {
            return offsetof(GraphicsPipelineKey, padding);
        }
    }
};
static_assert(std::has_unique_object_representations_v<GraphicsPipelineKey>);
static_assert(std::is_trivially_copyable_v<GraphicsPipelineKey>);
static_assert(std::is_trivially_constructible_v<GraphicsPipelineKey>);

class GraphicsPipeline {
public:
    explicit GraphicsPipeline(const Device& device, TextureCache& texture_cache_,
                              BufferCache& buffer_cache_, Tegra::MemoryManager& gpu_memory_,
                              Tegra::Engines::Maxwell3D& maxwell3d_,
                              ProgramManager& program_manager_, StateTracker& state_tracker_,
                              ShaderWorker* thread_worker, VideoCore::ShaderNotify* shader_notify,
                              std::array<std::string, 5> sources,
                              std::array<std::vector<u32>, 5> sources_spirv,
                              const std::array<const Shader::Info*, 5>& infos,
                              const GraphicsPipelineKey& key_);

    void Configure(bool is_indexed) {
        configure_func(this, is_indexed);
    }

    void ConfigureTransformFeedback() const {
        if (num_xfb_attribs != 0) {
            ConfigureTransformFeedbackImpl();
        }
    }

    [[nodiscard]] const GraphicsPipelineKey& Key() const noexcept {
        return key;
    }

    [[nodiscard]] bool WritesGlobalMemory() const noexcept {
        return writes_global_memory;
    }

    [[nodiscard]] bool IsBuilt() const noexcept {
        return is_built.load(std::memory_order::relaxed);
    }

    template <typename Spec>
    static auto MakeConfigureSpecFunc() {
        return [](GraphicsPipeline* pipeline, bool is_indexed) {
            pipeline->ConfigureImpl<Spec>(is_indexed);
        };
    }

private:
    template <typename Spec>
    void ConfigureImpl(bool is_indexed);

    void ConfigureTransformFeedbackImpl() const;

    void GenerateTransformFeedbackState();

    TextureCache& texture_cache;
    BufferCache& buffer_cache;
    Tegra::MemoryManager& gpu_memory;
    Tegra::Engines::Maxwell3D& maxwell3d;
    ProgramManager& program_manager;
    StateTracker& state_tracker;
    const GraphicsPipelineKey key;

    void (*configure_func)(GraphicsPipeline*, bool){};

    std::array<OGLProgram, 5> source_programs;
    std::array<OGLAssemblyProgram, 5> assembly_programs;
    u32 enabled_stages_mask{};

    std::array<Shader::Info, 5> stage_infos{};
    std::array<u32, 5> enabled_uniform_buffer_masks{};
    VideoCommon::UniformBufferSizes uniform_buffer_sizes{};
    std::array<u32, 5> base_uniform_bindings{};
    std::array<u32, 5> base_storage_bindings{};
    std::array<u32, 5> num_texture_buffers{};
    std::array<u32, 5> num_image_buffers{};

    bool use_storage_buffers{};
    bool writes_global_memory{};
    std::atomic_bool is_built{false};

    static constexpr std::size_t XFB_ENTRY_STRIDE = 3;
    GLsizei num_xfb_attribs{};
    GLsizei num_xfb_strides{};
    std::array<GLint, 128 * XFB_ENTRY_STRIDE * Maxwell::NumTransformFeedbackBuffers> xfb_attribs{};
    std::array<GLint, Maxwell::NumTransformFeedbackBuffers> xfb_streams{};
};

} // namespace OpenGL

namespace std {
template <>
struct hash<OpenGL::GraphicsPipelineKey> {
    size_t operator()(const OpenGL::GraphicsPipelineKey& k) const noexcept {
        return k.Hash();
    }
};
} // namespace std
