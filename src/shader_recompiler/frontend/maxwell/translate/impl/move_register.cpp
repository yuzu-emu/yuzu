// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/maxwell/opcodes.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
union MOV {
    u64 raw;
    BitField<0, 8, IR::Reg> dest_reg;
    BitField<20, 8, IR::Reg> src_reg;
    BitField<39, 4, u64> mask;
};

void CheckMask(MOV mov) {
    if (mov.mask != 0xf) {
        throw NotImplementedException("Non-full move mask");
    }
}
} // Anonymous namespace

void TranslatorVisitor::MOV_reg(u64 insn) {
    const MOV mov{insn};
    CheckMask(mov);
    X(mov.dest_reg, X(mov.src_reg));
}

void TranslatorVisitor::MOV_cbuf(u64 insn) {
    const MOV mov{insn};
    CheckMask(mov);
    X(mov.dest_reg, GetCbuf(insn));
}

void TranslatorVisitor::MOV_imm(u64 insn) {
    const MOV mov{insn};
    CheckMask(mov);
    X(mov.dest_reg, GetImm20(insn));
}

} // namespace Shader::Maxwell
