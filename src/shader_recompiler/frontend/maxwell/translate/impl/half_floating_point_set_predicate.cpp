// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/frontend/maxwell/translate/impl/half_floating_point_helper.h"

namespace Shader::Maxwell {
namespace {
void HSETP2(TranslatorVisitor& v, u64 insn, const IR::U32& src_b, bool neg_b, bool abs_b,
            Swizzle swizzle_b, FPCompareOp compare_op, bool h_and) {
    union {
        u64 insn;
        BitField<8, 8, IR::Reg> src_a_reg;
        BitField<3, 3, IR::Pred> dest_pred_a;
        BitField<0, 3, IR::Pred> dest_pred_b;
        BitField<39, 3, IR::Pred> pred;
        BitField<42, 1, u64> neg_pred;
        BitField<43, 1, u64> neg_a;
        BitField<45, 2, BooleanOp> bop;
        BitField<44, 1, u64> abs_a;
        BitField<6, 1, u64> ftz;
        BitField<47, 2, Swizzle> swizzle_a;
    } const hsetp2{insn};

    auto [lhs_a, rhs_a]{Extract(v.ir, v.X(hsetp2.src_a_reg), hsetp2.swizzle_a)};
    auto [lhs_b, rhs_b]{Extract(v.ir, src_b, swizzle_b)};

    if (lhs_a.Type() != lhs_b.Type()) {
        if (lhs_a.Type() == IR::Type::F16) {
            lhs_a = v.ir.FPConvert(32, lhs_a);
            rhs_a = v.ir.FPConvert(32, rhs_a);
        }
        if (lhs_b.Type() == IR::Type::F16) {
            lhs_b = v.ir.FPConvert(32, lhs_b);
            rhs_b = v.ir.FPConvert(32, rhs_b);
        }
    }

    lhs_a = v.ir.FPAbsNeg(lhs_a, hsetp2.abs_a != 0, hsetp2.neg_a != 0);
    rhs_a = v.ir.FPAbsNeg(rhs_a, hsetp2.abs_a != 0, hsetp2.neg_a != 0);

    lhs_b = v.ir.FPAbsNeg(lhs_b, abs_b, neg_b);
    rhs_b = v.ir.FPAbsNeg(rhs_b, abs_b, neg_b);

    const IR::FpControl control{
        .no_contraction = false,
        .rounding = IR::FpRounding::DontCare,
        .fmz_mode = (hsetp2.ftz != 0 ? IR::FmzMode::FTZ : IR::FmzMode::None),
    };

    IR::U1 pred{v.ir.GetPred(hsetp2.pred)};
    if (hsetp2.neg_pred != 0) {
        pred = v.ir.LogicalNot(pred);
    }
    const IR::U1 cmp_result_lhs{FloatingPointCompare(v.ir, lhs_a, lhs_b, compare_op, control)};
    const IR::U1 cmp_result_rhs{FloatingPointCompare(v.ir, rhs_a, rhs_b, compare_op, control)};
    const IR::U1 bop_result_lhs{PredicateCombine(v.ir, cmp_result_lhs, pred, hsetp2.bop)};
    const IR::U1 bop_result_rhs{PredicateCombine(v.ir, cmp_result_rhs, pred, hsetp2.bop)};

    if (h_and) {
        auto result = v.ir.LogicalAnd(bop_result_lhs, bop_result_rhs);
        v.ir.SetPred(hsetp2.dest_pred_a, result);
        v.ir.SetPred(hsetp2.dest_pred_b, v.ir.LogicalNot(result));
    } else {
        v.ir.SetPred(hsetp2.dest_pred_a, bop_result_lhs);
        v.ir.SetPred(hsetp2.dest_pred_b, bop_result_rhs);
    }
}
} // Anonymous namespace

void TranslatorVisitor::HSETP2_reg(u64 insn) {
    union {
        u64 insn;
        BitField<30, 1, u64> abs_b;
        BitField<49, 1, u64> h_and;
        BitField<31, 1, u64> neg_b;
        BitField<35, 4, FPCompareOp> compare_op;
        BitField<28, 2, Swizzle> swizzle_b;
    } const hsetp2{insn};
    HSETP2(*this, insn, GetReg20(insn), hsetp2.neg_b != 0, hsetp2.abs_b != 0, hsetp2.swizzle_b,
           hsetp2.compare_op, hsetp2.h_and != 0);
}

void TranslatorVisitor::HSETP2_cbuf(u64 insn) {
    union {
        u64 insn;
        BitField<53, 1, u64> h_and;
        BitField<54, 1, u64> abs_b;
        BitField<56, 1, u64> neg_b;
        BitField<49, 4, FPCompareOp> compare_op;
    } const hsetp2{insn};

    HSETP2(*this, insn, GetCbuf(insn), hsetp2.neg_b != 0, hsetp2.abs_b != 0, Swizzle::F32,
           hsetp2.compare_op, hsetp2.h_and != 0);
}

void TranslatorVisitor::HSETP2_imm(u64 insn) {
    union {
        u64 insn;
        BitField<53, 1, u64> h_and;
        BitField<54, 1, u64> ftz;
        BitField<49, 4, FPCompareOp> compare_op;
        BitField<56, 1, u64> neg_high;
        BitField<30, 9, u64> high;
        BitField<29, 1, u64> neg_low;
        BitField<20, 9, u64> low;
    } const hsetp2{insn};

    const u32 imm{static_cast<u32>(hsetp2.low << 6) |
                  static_cast<u32>((hsetp2.neg_low != 0 ? 1 : 0) << 15) |
                  static_cast<u32>(hsetp2.high << 22) |
                  static_cast<u32>((hsetp2.neg_high != 0 ? 1 : 0) << 31)};

    HSETP2(*this, insn, ir.Imm32(imm), false, false, Swizzle::H1_H0, hsetp2.compare_op,
           hsetp2.h_and != 0);
}

} // namespace Shader::Maxwell
