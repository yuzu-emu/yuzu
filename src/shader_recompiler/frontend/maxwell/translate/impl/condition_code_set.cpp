// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/common_funcs.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {

void TranslatorVisitor::CSET(u64 insn) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 5, IR::FlowTest> cc_test;
        BitField<39, 3, IR::Pred> bop_pred;
        BitField<42, 1, u64> neg_bop_pred;
        BitField<44, 1, u64> bf;
        BitField<45, 2, BooleanOp> bop;
        BitField<47, 1, u64> cc;
    } const cset{insn};

    const IR::U32 one_mask{ir.Imm32(-1)};
    const IR::U32 fp_one{ir.Imm32(0x3f800000)};
    const IR::U32 zero{ir.Imm32(0)};
    const IR::U32 pass_result{cset.bf == 0 ? one_mask : fp_one};
    const IR::U1 cc_test_result{ir.GetFlowTestResult(cset.cc_test)};
    const IR::U1 bop_pred{ir.GetPred(cset.bop_pred, cset.neg_bop_pred != 0)};
    const IR::U1 pred_result{PredicateCombine(ir, cc_test_result, bop_pred, cset.bop)};
    const IR::U32 result{ir.Select(pred_result, pass_result, zero)};
    X(cset.dest_reg, result);
    if (cset.cc != 0) {
        const IR::U1 is_zero{ir.IEqual(result, zero)};
        SetZFlag(is_zero);
        if (cset.bf != 0) {
            ResetSFlag();
        } else {
            SetSFlag(ir.LogicalNot(is_zero));
        }
        ResetOFlag();
        ResetCFlag();
    }
}

void TranslatorVisitor::CSETP(u64 insn) {
    union {
        u64 raw;
        BitField<0, 3, IR::Pred> dest_pred_b;
        BitField<3, 3, IR::Pred> dest_pred_a;
        BitField<8, 5, IR::FlowTest> cc_test;
        BitField<39, 3, IR::Pred> bop_pred;
        BitField<42, 1, u64> neg_bop_pred;
        BitField<45, 2, BooleanOp> bop;
    } const csetp{insn};

    const BooleanOp bop{csetp.bop};
    const IR::U1 bop_pred{ir.GetPred(csetp.bop_pred, csetp.neg_bop_pred != 0)};
    const IR::U1 cc_test_result{ir.GetFlowTestResult(csetp.cc_test)};
    const IR::U1 result_a{PredicateCombine(ir, cc_test_result, bop_pred, bop)};
    const IR::U1 result_b{PredicateCombine(ir, ir.LogicalNot(cc_test_result), bop_pred, bop)};
    ir.SetPred(csetp.dest_pred_a, result_a);
    ir.SetPred(csetp.dest_pred_b, result_b);
}

} // namespace Shader::Maxwell
