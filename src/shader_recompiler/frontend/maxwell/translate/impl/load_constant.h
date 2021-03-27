// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/ir/reg.h"

namespace Shader::Maxwell::LDC {

enum class Mode : u64 {
    Default,
    IL,
    IS,
    ISL,
};

enum class Size : u64 {
    U8,
    S8,
    U16,
    S16,
    B32,
    B64,
};

union Encoding {
    u64 raw;
    BitField<0, 8, IR::Reg> dest_reg;
    BitField<8, 8, IR::Reg> src_reg;
    BitField<20, 16, s64> offset;
    BitField<36, 5, u64> index;
    BitField<44, 2, Mode> mode;
    BitField<48, 3, Size> size;
};

} // namespace Shader::Maxwell::LDC
