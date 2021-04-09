// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/common_funcs.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
void TranslatorVisitor::PSET(u64 insn) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<12, 3, IR::Pred> pred_a;
        BitField<15, 1, u64> neg_pred_a;
        BitField<24, 2, BooleanOp> bop_1;
        BitField<29, 3, IR::Pred> pred_b;
        BitField<32, 1, u64> neg_pred_b;
        BitField<39, 3, IR::Pred> pred_c;
        BitField<42, 1, u64> neg_pred_c;
        BitField<44, 1, u64> bf;
        BitField<45, 2, BooleanOp> bop_2;
        BitField<47, 1, u64> cc;
    } const pset{insn};

    const IR::U1 pred_a{ir.GetPred(pset.pred_a, pset.neg_pred_a != 0)};
    const IR::U1 pred_b{ir.GetPred(pset.pred_b, pset.neg_pred_b != 0)};
    const IR::U1 pred_c{ir.GetPred(pset.pred_c, pset.neg_pred_c != 0)};

    const IR::U1 res_1{PredicateCombine(ir, pred_a, pred_b, pset.bop_1)};
    const IR::U1 res_2{PredicateCombine(ir, res_1, pred_c, pset.bop_2)};

    const IR::U32 true_result{pset.bf != 0 ? ir.Imm32(0x3f800000) : ir.Imm32(-1)};
    const IR::U32 zero{ir.Imm32(0)};

    const IR::U32 result{ir.Select(res_2, true_result, zero)};

    X(pset.dest_reg, result);
    if (pset.cc != 0) {
        const IR::U1 is_zero{ir.IEqual(result, zero)};
        SetZFlag(is_zero);
        if (pset.bf != 0) {
            ResetSFlag();
        } else {
            SetSFlag(ir.LogicalNot(is_zero));
        }
        ResetOFlag();
        ResetCFlag();
    }
}

} // namespace Shader::Maxwell
