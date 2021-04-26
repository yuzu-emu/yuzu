// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <bitset>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glad/glad.h>

#include "common/common_types.h"
#include "video_core/engines/shader_type.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/shader_cache.h"

namespace Tegra {
class MemoryManager;
}

namespace Core::Frontend {
class EmuWindow;
}

namespace OpenGL {

class Device;
class RasterizerOpenGL;

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
    std::array<u8, Maxwell::NumRenderTargets> color_formats;
    union {
        u32 raw;
        BitField<0, 1, u32> xfb_enabled;
        BitField<1, 1, u32> early_z;
        BitField<2, 4, Maxwell::PrimitiveTopology> gs_input_topology;
        BitField<6, 2, u32> tessellation_primitive;
        BitField<8, 2, u32> tessellation_spacing;
        BitField<10, 1, u32> tessellation_clockwise;
    };
    u32 padding;
    TransformFeedbackState xfb_state;

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
private:
};

class ShaderCache : public VideoCommon::ShaderCache {
public:
    explicit ShaderCache(RasterizerOpenGL& rasterizer_, Core::Frontend::EmuWindow& emu_window_,
                         Tegra::GPU& gpu_, Tegra::Engines::Maxwell3D& maxwell3d_,
                         Tegra::Engines::KeplerCompute& kepler_compute_,
                         Tegra::MemoryManager& gpu_memory_, const Device& device_);
    ~ShaderCache();

private:
    Core::Frontend::EmuWindow& emu_window;
    Tegra::GPU& gpu;
    const Device& device;
};

} // namespace OpenGL
