// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Shader {

enum class Stage : u32 {
    VertexB,
    TessellationControl,
    TessellationEval,
    Geometry,
    Fragment,

    Compute,

    VertexA,
};
constexpr u32 MaxStageTypes = 6;

[[nodiscard]] constexpr Stage StageFromIndex(size_t index) noexcept {
    return static_cast<Stage>(static_cast<size_t>(Stage::VertexB) + index);
}

} // namespace Shader
