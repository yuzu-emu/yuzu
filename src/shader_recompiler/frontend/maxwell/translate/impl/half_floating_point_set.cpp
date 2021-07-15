// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/frontend/maxwell/translate/impl/half_floating_point_helper.h"

namespace Shader::Maxwell {
namespace {
void HSET2(TranslatorVisitor& v, u64 insn, const IR::U32& src_b, bool bf, bool ftz, bool neg_b,
           bool abs_b, FPCompareOp compare_op, Swizzle swizzle_b) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_a_reg;
        BitField<39, 3, IR::Pred> pred;
        BitField<42, 1, u64> neg_pred;
        BitField<43, 1, u64> neg_a;
        BitField<45, 2, BooleanOp> bop;
        BitField<44, 1, u64> abs_a;
        BitField<47, 2, Swizzle> swizzle_a;
    } const hset2{insn};

    auto [lhs_a, rhs_a]{Extract(v.ir, v.X(hset2.src_a_reg), hset2.swizzle_a)};
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

    lhs_a = v.ir.FPAbsNeg(lhs_a, hset2.abs_a != 0, hset2.neg_a != 0);
    rhs_a = v.ir.FPAbsNeg(rhs_a, hset2.abs_a != 0, hset2.neg_a != 0);

    lhs_b = v.ir.FPAbsNeg(lhs_b, abs_b, neg_b);
    rhs_b = v.ir.FPAbsNeg(rhs_b, abs_b, neg_b);

    const IR::FpControl control{
        .no_contraction = false,
        .rounding = IR::FpRounding::DontCare,
        .fmz_mode = (ftz ? IR::FmzMode::FTZ : IR::FmzMode::None),
    };

    IR::U1 pred{v.ir.GetPred(hset2.pred)};
    if (hset2.neg_pred != 0) {
        pred = v.ir.LogicalNot(pred);
    }
    const IR::U1 cmp_result_lhs{FloatingPointCompare(v.ir, lhs_a, lhs_b, compare_op, control)};
    const IR::U1 cmp_result_rhs{FloatingPointCompare(v.ir, rhs_a, rhs_b, compare_op, control)};
    const IR::U1 bop_result_lhs{PredicateCombine(v.ir, cmp_result_lhs, pred, hset2.bop)};
    const IR::U1 bop_result_rhs{PredicateCombine(v.ir, cmp_result_rhs, pred, hset2.bop)};

    const u32 true_value = bf ? 0x3c00 : 0xffff;
    const IR::U32 true_val_lhs{v.ir.Imm32(true_value)};
    const IR::U32 true_val_rhs{v.ir.Imm32(true_value << 16)};
    const IR::U32 fail_result{v.ir.Imm32(0)};
    const IR::U32 result_lhs{v.ir.Select(bop_result_lhs, true_val_lhs, fail_result)};
    const IR::U32 result_rhs{v.ir.Select(bop_result_rhs, true_val_rhs, fail_result)};

    v.X(hset2.dest_reg, IR::U32{v.ir.BitwiseOr(result_lhs, result_rhs)});
}
} // Anonymous namespace

void TranslatorVisitor::HSET2_reg(u64 insn) {
    union {
        u64 insn;
        BitField<30, 1, u64> abs_b;
        BitField<49, 1, u64> bf;
        BitField<31, 1, u64> neg_b;
        BitField<50, 1, u64> ftz;
        BitField<35, 4, FPCompareOp> compare_op;
        BitField<28, 2, Swizzle> swizzle_b;
    } const hset2{insn};

    HSET2(*this, insn, GetReg20(insn), hset2.bf != 0, hset2.ftz != 0, hset2.neg_b != 0,
          hset2.abs_b != 0, hset2.compare_op, hset2.swizzle_b);
}

void TranslatorVisitor::HSET2_cbuf(u64 insn) {
    union {
        u64 insn;
        BitField<53, 1, u64> bf;
        BitField<56, 1, u64> neg_b;
        BitField<54, 1, u64> ftz;
        BitField<49, 4, FPCompareOp> compare_op;
    } const hset2{insn};

    HSET2(*this, insn, GetCbuf(insn), hset2.bf != 0, hset2.ftz != 0, hset2.neg_b != 0, false,
          hset2.compare_op, Swizzle::F32);
}

void TranslatorVisitor::HSET2_imm(u64 insn) {
    union {
        u64 insn;
        BitField<53, 1, u64> bf;
        BitField<54, 1, u64> ftz;
        BitField<49, 4, FPCompareOp> compare_op;
        BitField<56, 1, u64> neg_high;
        BitField<30, 9, u64> high;
        BitField<29, 1, u64> neg_low;
        BitField<20, 9, u64> low;
    } const hset2{insn};

    const u32 imm{
        static_cast<u32>(hset2.low << 6) | static_cast<u32>((hset2.neg_low != 0 ? 1 : 0) << 15) |
        static_cast<u32>(hset2.high << 22) | static_cast<u32>((hset2.neg_high != 0 ? 1 : 0) << 31)};

    HSET2(*this, insn, ir.Imm32(imm), hset2.bf != 0, hset2.ftz != 0, false, false, hset2.compare_op,
          Swizzle::H1_H0);
}

} // namespace Shader::Maxwell
