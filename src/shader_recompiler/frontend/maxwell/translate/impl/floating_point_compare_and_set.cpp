// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/common_funcs.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
void FSET(TranslatorVisitor& v, u64 insn, const IR::F32& src_b) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_a_reg;
        BitField<39, 3, IR::Pred> pred;
        BitField<42, 1, u64> neg_pred;
        BitField<43, 1, u64> negate_a;
        BitField<44, 1, u64> abs_b;
        BitField<45, 2, BooleanOp> bop;
        BitField<47, 1, u64> cc;
        BitField<48, 4, FPCompareOp> compare_op;
        BitField<52, 1, u64> bf;
        BitField<53, 1, u64> negate_b;
        BitField<54, 1, u64> abs_a;
        BitField<55, 1, u64> ftz;
    } const fset{insn};

    const IR::F32 op_a{v.ir.FPAbsNeg(v.F(fset.src_a_reg), fset.abs_a != 0, fset.negate_a != 0)};
    const IR::F32 op_b = v.ir.FPAbsNeg(src_b, fset.abs_b != 0, fset.negate_b != 0);
    const IR::FpControl control{
        .no_contraction = false,
        .rounding = IR::FpRounding::DontCare,
        .fmz_mode = (fset.ftz != 0 ? IR::FmzMode::FTZ : IR::FmzMode::None),
    };

    IR::U1 pred{v.ir.GetPred(fset.pred)};
    if (fset.neg_pred != 0) {
        pred = v.ir.LogicalNot(pred);
    }
    const IR::U1 cmp_result{FloatingPointCompare(v.ir, op_a, op_b, fset.compare_op, control)};
    const IR::U1 bop_result{PredicateCombine(v.ir, cmp_result, pred, fset.bop)};

    const IR::U32 one_mask{v.ir.Imm32(-1)};
    const IR::U32 fp_one{v.ir.Imm32(0x3f800000)};
    const IR::U32 zero{v.ir.Imm32(0)};
    const IR::U32 pass_result{fset.bf == 0 ? one_mask : fp_one};
    const IR::U32 result{v.ir.Select(bop_result, pass_result, zero)};

    v.X(fset.dest_reg, result);
    if (fset.cc != 0) {
        const IR::U1 is_zero{v.ir.IEqual(result, zero)};
        v.SetZFlag(is_zero);
        if (fset.bf != 0) {
            v.ResetSFlag();
        } else {
            v.SetSFlag(v.ir.LogicalNot(is_zero));
        }
        v.ResetCFlag();
        v.ResetOFlag();
    }
}
} // Anonymous namespace

void TranslatorVisitor::FSET_reg(u64 insn) {
    FSET(*this, insn, GetFloatReg20(insn));
}

void TranslatorVisitor::FSET_cbuf(u64 insn) {
    FSET(*this, insn, GetFloatCbuf(insn));
}

void TranslatorVisitor::FSET_imm(u64 insn) {
    FSET(*this, insn, GetFloatImm20(insn));
}

} // namespace Shader::Maxwell
