// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/ir/modifiers.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/common_encoding.h"

namespace Shader::Maxwell {

enum class FpRounding : u64 {
    RN,
    RM,
    RP,
    RZ,
};

enum class FmzMode : u64 {
    None,
    FTZ,
    FMZ,
    INVALIDFMZ3,
};

inline IR::FpRounding CastFpRounding(FpRounding fp_rounding) {
    switch (fp_rounding) {
    case FpRounding::RN:
        return IR::FpRounding::RN;
    case FpRounding::RM:
        return IR::FpRounding::RM;
    case FpRounding::RP:
        return IR::FpRounding::RP;
    case FpRounding::RZ:
        return IR::FpRounding::RZ;
    }
    throw NotImplementedException("Invalid floating-point rounding {}", fp_rounding);
}

inline IR::FmzMode CastFmzMode(FmzMode fmz_mode) {
    switch (fmz_mode) {
    case FmzMode::None:
        return IR::FmzMode::None;
    case FmzMode::FTZ:
        return IR::FmzMode::FTZ;
    case FmzMode::FMZ:
        // FMZ is manually handled in the instruction
        return IR::FmzMode::FTZ;
    case FmzMode::INVALIDFMZ3:
        break;
    }
    throw NotImplementedException("Invalid FMZ mode {}", fmz_mode);
}

} // namespace Shader::Maxwell
