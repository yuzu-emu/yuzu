// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Shader::IR {

enum class FmzMode : u8 {
    DontCare, // Not specified for this instruction
    FTZ,      // Flush denorms to zero, NAN is propagated (D3D11, NVN, GL, VK)
    FMZ,      // Flush denorms to zero, x * 0 == 0 (D3D9)
    None,     // Denorms are not flushed, NAN is propagated (nouveau)
};

enum class FpRounding : u8 {
    DontCare, // Not specified for this instruction
    RN,       // Round to nearest even,
    RM,       // Round towards negative infinity
    RP,       // Round towards positive infinity
    RZ,       // Round towards zero
};

struct FpControl {
    bool no_contraction{false};
    FpRounding rounding{FpRounding::DontCare};
    FmzMode fmz_mode{FmzMode::DontCare};
};
static_assert(sizeof(FpControl) <= sizeof(u32));

} // namespace Shader::IR
