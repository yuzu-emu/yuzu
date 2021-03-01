// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
void FLO(TranslatorVisitor& v, u64 insn, const IR::U32& src) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<40, 1, u64> tilde;
        BitField<41, 1, u64> shift;
        BitField<48, 1, u64> is_signed;
    } const flo{insn};

    const bool invert{flo.tilde != 0};
    const bool is_signed{flo.is_signed != 0};
    const bool shift_op{flo.shift != 0};

    const IR::U32 operand{invert ? v.ir.BitwiseNot(src) : src};
    const IR::U32 find_result{is_signed ? v.ir.FindSMsb(operand) : v.ir.FindUMsb(operand)};
    const IR::U1 find_fail{v.ir.IEqual(find_result, v.ir.Imm32(-1))};
    const IR::U32 offset{v.ir.Imm32(31)};
    const IR::U32 success_result{shift_op ? IR::U32{v.ir.ISub(offset, find_result)} : find_result};

    const IR::U32 result{v.ir.Select(find_fail, find_result, success_result)};
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
