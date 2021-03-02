// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/common_funcs.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
void ISETP(TranslatorVisitor& v, u64 insn, const IR::U32& op_b) {
    union {
        u64 raw;
        BitField<0, 3, IR::Pred> dest_pred_b;
        BitField<3, 3, IR::Pred> dest_pred_a;
        BitField<8, 8, IR::Reg> src_reg_a;
        BitField<39, 3, IR::Pred> bop_pred;
        BitField<42, 1, u64> neg_bop_pred;
        BitField<45, 2, BooleanOp> bop;
        BitField<48, 1, u64> is_signed;
        BitField<49, 3, CompareOp> compare_op;
    } const isetp{insn};

    const BooleanOp bop{isetp.bop};
    const CompareOp compare_op{isetp.compare_op};
    const IR::U32 op_a{v.X(isetp.src_reg_a)};
    const IR::U1 comparison{IntegerCompare(v.ir, op_a, op_b, compare_op, isetp.is_signed != 0)};
    const IR::U1 bop_pred{v.ir.GetPred(isetp.bop_pred, isetp.neg_bop_pred != 0)};
    const IR::U1 result_a{PredicateCombine(v.ir, comparison, bop_pred, bop)};
    const IR::U1 result_b{PredicateCombine(v.ir, v.ir.LogicalNot(comparison), bop_pred, bop)};
    v.ir.SetPred(isetp.dest_pred_a, result_a);
    v.ir.SetPred(isetp.dest_pred_b, result_b);
}
} // Anonymous namespace

void TranslatorVisitor::ISETP_reg(u64 insn) {
    ISETP(*this, insn, GetReg20(insn));
}

void TranslatorVisitor::ISETP_cbuf(u64 insn) {
    ISETP(*this, insn, GetCbuf(insn));
}

void TranslatorVisitor::ISETP_imm(u64 insn) {
    ISETP(*this, insn, GetImm20(insn));
}

} // namespace Shader::Maxwell
