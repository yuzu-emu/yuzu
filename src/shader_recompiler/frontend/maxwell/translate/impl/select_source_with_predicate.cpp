// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {

void SEL(TranslatorVisitor& v, u64 insn, const IR::U32& src) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_reg;
        BitField<39, 3, IR::Pred> pred;
        BitField<42, 1, u64> neg_pred;
    } const sel{insn};

    const IR::U1 pred = v.ir.GetPred(sel.pred);
    IR::U32 op_a{v.X(sel.src_reg)};
    IR::U32 op_b{src};
    if (sel.neg_pred != 0) {
        std::swap(op_a, op_b);
    }
    const IR::U32 result{v.ir.Select(pred, op_a, op_b)};

    v.X(sel.dest_reg, result);
}
} // Anonymous namespace

void TranslatorVisitor::SEL_reg(u64 insn) {
    SEL(*this, insn, GetReg20(insn));
}

void TranslatorVisitor::SEL_cbuf(u64 insn) {
    SEL(*this, insn, GetCbuf(insn));
}

void TranslatorVisitor::SEL_imm(u64 insn) {
    SEL(*this, insn, GetImm20(insn));
}
} // namespace Shader::Maxwell
