// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/common_funcs.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
void ICMP(TranslatorVisitor& v, u64 insn, const IR::U32& src_a, const IR::U32& operand) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_reg;
        BitField<48, 1, u64> is_signed;
        BitField<49, 3, CompareOp> compare_op;
    } const icmp{insn};

    const IR::U32 zero{v.ir.Imm32(0)};
    const bool is_signed{icmp.is_signed != 0};
    const IR::U1 cmp_result{IntegerCompare(v.ir, operand, zero, icmp.compare_op, is_signed)};

    const IR::U32 src_reg{v.X(icmp.src_reg)};
    const IR::U32 result{v.ir.Select(cmp_result, src_reg, src_a)};

    v.X(icmp.dest_reg, result);
}
} // Anonymous namespace

void TranslatorVisitor::ICMP_reg(u64 insn) {
    ICMP(*this, insn, GetReg20(insn), GetReg39(insn));
}

void TranslatorVisitor::ICMP_rc(u64 insn) {
    ICMP(*this, insn, GetReg39(insn), GetCbuf(insn));
}

void TranslatorVisitor::ICMP_cr(u64 insn) {
    ICMP(*this, insn, GetCbuf(insn), GetReg39(insn));
}

void TranslatorVisitor::ICMP_imm(u64 insn) {
    ICMP(*this, insn, GetImm20(insn), GetReg39(insn));
}

} // namespace Shader::Maxwell
