// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/common_funcs.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
void TranslatorVisitor::PSETP(u64 insn) {
    union {
        u64 raw;
        BitField<0, 3, IR::Pred> dest_pred_b;
        BitField<3, 3, IR::Pred> dest_pred_a;
        BitField<12, 3, IR::Pred> pred_a;
        BitField<15, 1, u64> neg_pred_a;
        BitField<24, 2, BooleanOp> bop_1;
        BitField<29, 3, IR::Pred> pred_b;
        BitField<32, 1, u64> neg_pred_b;
        BitField<39, 3, IR::Pred> pred_c;
        BitField<42, 1, u64> neg_pred_c;
        BitField<45, 2, BooleanOp> bop_2;
    } const pset{insn};

    const IR::U1 pred_a{ir.GetPred(pset.pred_a, pset.neg_pred_a != 0)};
    const IR::U1 pred_b{ir.GetPred(pset.pred_b, pset.neg_pred_b != 0)};
    const IR::U1 pred_c{ir.GetPred(pset.pred_c, pset.neg_pred_c != 0)};

    const IR::U1 lhs_a{PredicateCombine(ir, pred_a, pred_b, pset.bop_1)};
    const IR::U1 lhs_b{PredicateCombine(ir, ir.LogicalNot(pred_a), pred_b, pset.bop_1)};
    const IR::U1 result_a{PredicateCombine(ir, lhs_a, pred_c, pset.bop_2)};
    const IR::U1 result_b{PredicateCombine(ir, lhs_b, pred_c, pset.bop_2)};

    ir.SetPred(pset.dest_pred_a, result_a);
    ir.SetPred(pset.dest_pred_b, result_b);
}
} // namespace Shader::Maxwell
