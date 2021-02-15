// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Shader::IR {

enum class FmzMode : u8 {
    None, // Denorms are not flushed, NAN is propagated (nouveau)
    FTZ,  // Flush denorms to zero, NAN is propagated (D3D11, NVN, GL, VK)
    FMZ,  // Flush denorms to zero, x * 0 == 0 (D3D9)
};

enum class FpRounding : u8 {
    RN, // Round to nearest even,
    RM, // Round towards negative infinity
    RP, // Round towards positive infinity
    RZ, // Round towards zero
};

struct FpControl {
    bool no_contraction{false};
    FpRounding rounding{FpRounding::RN};
    FmzMode fmz_mode{FmzMode::FTZ};
};
static_assert(sizeof(FpControl) <= sizeof(u32));
} // namespace Shader::IR
