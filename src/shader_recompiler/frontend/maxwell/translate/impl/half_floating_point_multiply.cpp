// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/frontend/maxwell/translate/impl/half_floating_point_helper.h"

namespace Shader::Maxwell {
namespace {
void HMUL2(TranslatorVisitor& v, u64 insn, Merge merge, bool sat, bool abs_a, bool neg_a,
           Swizzle swizzle_a, bool abs_b, bool neg_b, Swizzle swizzle_b, const IR::U32& src_b,
           HalfPrecision precision) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_a;
    } const hmul2{insn};

    auto [lhs_a, rhs_a]{Extract(v.ir, v.X(hmul2.src_a), swizzle_a)};
    auto [lhs_b, rhs_b]{Extract(v.ir, src_b, swizzle_b)};
    const bool promotion{lhs_a.Type() != lhs_b.Type()};
    if (promotion) {
        if (lhs_a.Type() == IR::Type::F16) {
            lhs_a = v.ir.FPConvert(32, lhs_a);
            rhs_a = v.ir.FPConvert(32, rhs_a);
        }
        if (lhs_b.Type() == IR::Type::F16) {
            lhs_b = v.ir.FPConvert(32, lhs_b);
            rhs_b = v.ir.FPConvert(32, rhs_b);
        }
    }
    lhs_a = v.ir.FPAbsNeg(lhs_a, abs_a, neg_a);
    rhs_a = v.ir.FPAbsNeg(rhs_a, abs_a, neg_a);

    lhs_b = v.ir.FPAbsNeg(lhs_b, abs_b, neg_b);
    rhs_b = v.ir.FPAbsNeg(rhs_b, abs_b, neg_b);

    const IR::FpControl fp_control{
        .no_contraction = true,
        .rounding = IR::FpRounding::DontCare,
        .fmz_mode = HalfPrecision2FmzMode(precision),
    };
    IR::F16F32F64 lhs{v.ir.FPMul(lhs_a, lhs_b, fp_control)};
    IR::F16F32F64 rhs{v.ir.FPMul(rhs_a, rhs_b, fp_control)};
    if (precision == HalfPrecision::FMZ && !sat) {
        // Do not implement FMZ if SAT is enabled, as it does the logic for us.
        // On D3D9 mode, anything * 0 is zero, even NAN and infinity
        const IR::F32 zero{v.ir.Imm32(0.0f)};
        const IR::U1 lhs_zero_a{v.ir.FPEqual(lhs_a, zero)};
        const IR::U1 lhs_zero_b{v.ir.FPEqual(lhs_b, zero)};
        const IR::U1 lhs_any_zero{v.ir.LogicalOr(lhs_zero_a, lhs_zero_b)};
        lhs = IR::F16F32F64{v.ir.Select(lhs_any_zero, zero, lhs)};

        const IR::U1 rhs_zero_a{v.ir.FPEqual(rhs_a, zero)};
        const IR::U1 rhs_zero_b{v.ir.FPEqual(rhs_b, zero)};
        const IR::U1 rhs_any_zero{v.ir.LogicalOr(rhs_zero_a, rhs_zero_b)};
        rhs = IR::F16F32F64{v.ir.Select(rhs_any_zero, zero, rhs)};
    }
    if (sat) {
        lhs = v.ir.FPSaturate(lhs);
        rhs = v.ir.FPSaturate(rhs);
    }
    if (promotion) {
        lhs = v.ir.FPConvert(16, lhs);
        rhs = v.ir.FPConvert(16, rhs);
    }
    v.X(hmul2.dest_reg, MergeResult(v.ir, hmul2.dest_reg, lhs, rhs, merge));
}

void HMUL2(TranslatorVisitor& v, u64 insn, bool sat, bool abs_a, bool neg_a, bool abs_b, bool neg_b,
           Swizzle swizzle_b, const IR::U32& src_b) {
    union {
        u64 raw;
        BitField<49, 2, Merge> merge;
        BitField<47, 2, Swizzle> swizzle_a;
        BitField<39, 2, HalfPrecision> precision;
    } const hmul2{insn};

    HMUL2(v, insn, hmul2.merge, sat, abs_a, neg_a, hmul2.swizzle_a, abs_b, neg_b, swizzle_b, src_b,
          hmul2.precision);
}
} // Anonymous namespace

void TranslatorVisitor::HMUL2_reg(u64 insn) {
    union {
        u64 raw;
        BitField<32, 1, u64> sat;
        BitField<31, 1, u64> neg_b;
        BitField<30, 1, u64> abs_b;
        BitField<44, 1, u64> abs_a;
        BitField<28, 2, Swizzle> swizzle_b;
    } const hmul2{insn};

    HMUL2(*this, insn, hmul2.sat != 0, hmul2.abs_a != 0, false, hmul2.abs_b != 0, hmul2.neg_b != 0,
          hmul2.swizzle_b, GetReg20(insn));
}

void TranslatorVisitor::HMUL2_cbuf(u64 insn) {
    union {
        u64 raw;
        BitField<52, 1, u64> sat;
        BitField<54, 1, u64> abs_b;
        BitField<43, 1, u64> neg_a;
        BitField<44, 1, u64> abs_a;
    } const hmul2{insn};

    HMUL2(*this, insn, hmul2.sat != 0, hmul2.abs_a != 0, hmul2.neg_a != 0, hmul2.abs_b != 0, false,
          Swizzle::F32, GetCbuf(insn));
}

void TranslatorVisitor::HMUL2_imm(u64 insn) {
    union {
        u64 raw;
        BitField<52, 1, u64> sat;
        BitField<56, 1, u64> neg_high;
        BitField<30, 9, u64> high;
        BitField<29, 1, u64> neg_low;
        BitField<20, 9, u64> low;
        BitField<43, 1, u64> neg_a;
        BitField<44, 1, u64> abs_a;
    } const hmul2{insn};

    const u32 imm{
        static_cast<u32>(hmul2.low << 6) | static_cast<u32>((hmul2.neg_low != 0 ? 1 : 0) << 15) |
        static_cast<u32>(hmul2.high << 22) | static_cast<u32>((hmul2.neg_high != 0 ? 1 : 0) << 31)};
    HMUL2(*this, insn, hmul2.sat != 0, hmul2.abs_a != 0, hmul2.neg_a != 0, false, false,
          Swizzle::H1_H0, ir.Imm32(imm));
}

void TranslatorVisitor::HMUL2_32I(u64 insn) {
    union {
        u64 raw;
        BitField<55, 2, HalfPrecision> precision;
        BitField<52, 1, u64> sat;
        BitField<53, 2, Swizzle> swizzle_a;
        BitField<20, 32, u64> imm32;
    } const hmul2{insn};

    const u32 imm{static_cast<u32>(hmul2.imm32)};
    HMUL2(*this, insn, Merge::H1_H0, hmul2.sat != 0, false, false, hmul2.swizzle_a, false, false,
          Swizzle::H1_H0, ir.Imm32(imm), hmul2.precision);
}

} // namespace Shader::Maxwell
