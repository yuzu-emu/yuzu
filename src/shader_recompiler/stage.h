// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Shader {

enum class Stage : u32 {
    Compute,
    VertexA,
    VertexB,
    TessellationControl,
    TessellationEval,
    Geometry,
    Fragment,
};

} // namespace Shader
