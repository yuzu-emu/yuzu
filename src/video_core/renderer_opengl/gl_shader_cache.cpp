// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/scope_exit.h"
#include "core/core.h"
#include "core/frontend/emu_window.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/engines/shader_type.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_shader_cache.h"
#include "video_core/renderer_opengl/gl_state_tracker.h"
#include "video_core/shader_cache.h"
#include "video_core/shader_notify.h"

namespace OpenGL {

Shader::Shader() = default;

Shader::~Shader() = default;

ShaderCacheOpenGL::ShaderCacheOpenGL(RasterizerOpenGL& rasterizer_,
                                     Core::Frontend::EmuWindow& emu_window_, Tegra::GPU& gpu_,
                                     Tegra::Engines::Maxwell3D& maxwell3d_,
                                     Tegra::Engines::KeplerCompute& kepler_compute_,
                                     Tegra::MemoryManager& gpu_memory_, const Device& device_)
    : ShaderCache{rasterizer_}, emu_window{emu_window_}, gpu{gpu_}, gpu_memory{gpu_memory_},
      maxwell3d{maxwell3d_}, kepler_compute{kepler_compute_}, device{device_} {}

ShaderCacheOpenGL::~ShaderCacheOpenGL() = default;

} // namespace OpenGL
