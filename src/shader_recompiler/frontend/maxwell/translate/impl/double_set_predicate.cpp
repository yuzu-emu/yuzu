// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/common_funcs.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
void DSETP(TranslatorVisitor& v, u64 insn, const IR::F64& src_b) {
    union {
        u64 insn;
        BitField<0, 3, IR::Pred> dest_pred_b;
        BitField<3, 3, IR::Pred> dest_pred_a;
        BitField<6, 1, u64> negate_b;
        BitField<7, 1, u64> abs_a;
        BitField<8, 8, IR::Reg> src_a_reg;
        BitField<39, 3, IR::Pred> bop_pred;
        BitField<42, 1, u64> neg_bop_pred;
        BitField<43, 1, u64> negate_a;
        BitField<44, 1, u64> abs_b;
        BitField<45, 2, BooleanOp> bop;
        BitField<48, 4, FPCompareOp> compare_op;
    } const dsetp{insn};

    const IR::F64 op_a{v.ir.FPAbsNeg(v.D(dsetp.src_a_reg), dsetp.abs_a != 0, dsetp.negate_a != 0)};
    const IR::F64 op_b{v.ir.FPAbsNeg(src_b, dsetp.abs_b != 0, dsetp.negate_b != 0)};

    const BooleanOp bop{dsetp.bop};
    const FPCompareOp compare_op{dsetp.compare_op};
    const IR::U1 comparison{FloatingPointCompare(v.ir, op_a, op_b, compare_op)};
    const IR::U1 bop_pred{v.ir.GetPred(dsetp.bop_pred, dsetp.neg_bop_pred != 0)};
    const IR::U1 result_a{PredicateCombine(v.ir, comparison, bop_pred, bop)};
    const IR::U1 result_b{PredicateCombine(v.ir, v.ir.LogicalNot(comparison), bop_pred, bop)};
    v.ir.SetPred(dsetp.dest_pred_a, result_a);
    v.ir.SetPred(dsetp.dest_pred_b, result_b);
}
} // Anonymous namespace

void TranslatorVisitor::DSETP_reg(u64 insn) {
    DSETP(*this, insn, GetDoubleReg20(insn));
}

void TranslatorVisitor::DSETP_cbuf(u64 insn) {
    DSETP(*this, insn, GetDoubleCbuf(insn));
}

void TranslatorVisitor::DSETP_imm(u64 insn) {
    DSETP(*this, insn, GetDoubleImm20(insn));
}

} // namespace Shader::Maxwell
