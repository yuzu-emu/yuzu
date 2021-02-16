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

class Shader {
public:
    explicit Shader();
    ~Shader();
};

class ShaderCacheOpenGL final : public VideoCommon::ShaderCache<Shader> {
public:
    explicit ShaderCacheOpenGL(RasterizerOpenGL& rasterizer_,
                               Core::Frontend::EmuWindow& emu_window_, Tegra::GPU& gpu,
                               Tegra::Engines::Maxwell3D& maxwell3d_,
                               Tegra::Engines::KeplerCompute& kepler_compute_,
                               Tegra::MemoryManager& gpu_memory_, const Device& device_);
    ~ShaderCacheOpenGL() override;

private:
    Core::Frontend::EmuWindow& emu_window;
    Tegra::GPU& gpu;
    Tegra::MemoryManager& gpu_memory;
    Tegra::Engines::Maxwell3D& maxwell3d;
    Tegra::Engines::KeplerCompute& kepler_compute;
    const Device& device;
};

} // namespace OpenGL
