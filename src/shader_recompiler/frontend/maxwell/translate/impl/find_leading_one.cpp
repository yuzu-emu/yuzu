// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
void FLO(TranslatorVisitor& v, u64 insn, IR::U32 src) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<40, 1, u64> tilde;
        BitField<41, 1, u64> shift;
        BitField<47, 1, u64> cc;
        BitField<48, 1, u64> is_signed;
    } const flo{insn};

    if (flo.cc != 0) {
        throw NotImplementedException("CC");
    }
    if (flo.tilde != 0) {
        src = v.ir.BitwiseNot(src);
    }
    IR::U32 result{flo.is_signed != 0 ? v.ir.FindSMsb(src) : v.ir.FindUMsb(src)};
    if (flo.shift != 0) {
        const IR::U1 not_found{v.ir.IEqual(result, v.ir.Imm32(-1))};
        result = IR::U32{v.ir.Select(not_found, result, v.ir.BitwiseXor(result, v.ir.Imm32(31)))};
    }
    v.X(flo.dest_reg, result);
}
} // Anonymous namespace

void TranslatorVisitor::FLO_reg(u64 insn) {
    FLO(*this, insn, GetReg20(insn));
}

void TranslatorVisitor::FLO_cbuf(u64 insn) {
    FLO(*this, insn, GetCbuf(insn));
}

void TranslatorVisitor::FLO_imm(u64 insn) {
    FLO(*this, insn, GetImm20(insn));
}
} // namespace Shader::Maxwell
