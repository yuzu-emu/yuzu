// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/opcodes.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {

enum class BitSize : u64 {
    B32,
    B64,
    B96,
    B128,
};

void TranslatorVisitor::AL2P(u64 inst) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> result_register;
        BitField<8, 8, IR::Reg> indexing_register;
        BitField<20, 11, s64> offset;
        BitField<47, 2, BitSize> bitsize;
    } al2p{inst};
    if (al2p.bitsize != BitSize::B32) {
        throw NotImplementedException("BitSize {}", al2p.bitsize.Value());
    }
    const IR::U32 converted_offset{ir.Imm32(static_cast<u32>(al2p.offset.Value()))};
    const IR::U32 result{ir.IAdd(X(al2p.indexing_register), converted_offset)};
    X(al2p.result_register, result);
}

} // namespace Shader::Maxwell
