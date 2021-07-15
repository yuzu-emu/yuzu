// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <vector>

#include "common/common_types.h"
#include "shader_recompiler/runtime_info.h"
#include "video_core/engines/maxwell_3d.h"

namespace VideoCommon {

struct TransformFeedbackState {
    struct Layout {
        u32 stream;
        u32 varying_count;
        u32 stride;
    };
    std::array<Layout, Tegra::Engines::Maxwell3D::Regs::NumTransformFeedbackBuffers> layouts;
    std::array<std::array<u8, 128>, Tegra::Engines::Maxwell3D::Regs::NumTransformFeedbackBuffers>
        varyings;
};

std::vector<Shader::TransformFeedbackVarying> MakeTransformFeedbackVaryings(
    const TransformFeedbackState& state);

} // namespace VideoCommon
