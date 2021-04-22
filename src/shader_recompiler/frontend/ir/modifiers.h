// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/shader_info.h"

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

union TextureInstInfo {
    u32 raw;
    BitField<0, 16, u32> descriptor_index;
    BitField<16, 3, TextureType> type;
    BitField<19, 1, u32> is_depth;
    BitField<20, 1, u32> has_bias;
    BitField<21, 1, u32> has_lod_clamp;
    BitField<22, 1, u32> relaxed_precision;
    BitField<23, 2, u32> gather_component;
    BitField<25, 2, u32> num_derivates;
    BitField<27, 3, ImageFormat> image_format;
};
static_assert(sizeof(TextureInstInfo) <= sizeof(u32));

} // namespace Shader::IR
