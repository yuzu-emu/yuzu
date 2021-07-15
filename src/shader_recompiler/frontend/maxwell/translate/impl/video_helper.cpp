// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/video_helper.h"

namespace Shader::Maxwell {

IR::U32 ExtractVideoOperandValue(IR::IREmitter& ir, const IR::U32& value, VideoWidth width,
                                 u32 selector, bool is_signed) {
    switch (width) {
    case VideoWidth::Byte:
    case VideoWidth::Unknown:
        return ir.BitFieldExtract(value, ir.Imm32(selector * 8), ir.Imm32(8), is_signed);
    case VideoWidth::Short:
        return ir.BitFieldExtract(value, ir.Imm32(selector * 16), ir.Imm32(16), is_signed);
    case VideoWidth::Word:
        return value;
    default:
        throw NotImplementedException("Unknown VideoWidth {}", width);
    }
}

VideoWidth GetVideoSourceWidth(VideoWidth width, bool is_immediate) {
    // immediates must be 16-bit format.
    return is_immediate ? VideoWidth::Short : width;
}

} // namespace Shader::Maxwell
