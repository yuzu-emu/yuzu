// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/frontend/maxwell/translate/impl/half_floating_point_helper.h"

namespace Shader::Maxwell {
namespace {
void HFMA2(TranslatorVisitor& v, u64 insn, Merge merge, Swizzle swizzle_a, bool neg_b, bool neg_c,
           Swizzle swizzle_b, Swizzle swizzle_c, const IR::U32& src_b, const IR::U32& src_c,
           bool sat, HalfPrecision precision) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_a;
    } const hfma2{insn};

    auto [lhs_a, rhs_a]{Extract(v.ir, v.X(hfma2.src_a), swizzle_a)};
    auto [lhs_b, rhs_b]{Extract(v.ir, src_b, swizzle_b)};
    auto [lhs_c, rhs_c]{Extract(v.ir, src_c, swizzle_c)};
    const bool promotion{lhs_a.Type() != lhs_b.Type() || lhs_a.Type() != lhs_c.Type()};
    if (promotion) {
        if (lhs_a.Type() == IR::Type::F16) {
            lhs_a = v.ir.FPConvert(32, lhs_a);
            rhs_a = v.ir.FPConvert(32, rhs_a);
        }
        if (lhs_b.Type() == IR::Type::F16) {
            lhs_b = v.ir.FPConvert(32, lhs_b);
            rhs_b = v.ir.FPConvert(32, rhs_b);
        }
        if (lhs_c.Type() == IR::Type::F16) {
            lhs_c = v.ir.FPConvert(32, lhs_c);
            rhs_c = v.ir.FPConvert(32, rhs_c);
        }
    }

    lhs_b = v.ir.FPAbsNeg(lhs_b, false, neg_b);
    rhs_b = v.ir.FPAbsNeg(rhs_b, false, neg_b);

    lhs_c = v.ir.FPAbsNeg(lhs_c, false, neg_c);
    rhs_c = v.ir.FPAbsNeg(rhs_c, false, neg_c);

    const IR::FpControl fp_control{
        .no_contraction = true,
        .rounding = IR::FpRounding::DontCare,
        .fmz_mode = HalfPrecision2FmzMode(precision),
    };
    IR::F16F32F64 lhs{v.ir.FPFma(lhs_a, lhs_b, lhs_c, fp_control)};
    IR::F16F32F64 rhs{v.ir.FPFma(rhs_a, rhs_b, rhs_c, fp_control)};
    if (precision == HalfPrecision::FMZ && !sat) {
        // Do not implement FMZ if SAT is enabled, as it does the logic for us.
        // On D3D9 mode, anything * 0 is zero, even NAN and infinity
        const IR::F32 zero{v.ir.Imm32(0.0f)};
        const IR::U1 lhs_zero_a{v.ir.FPEqual(lhs_a, zero)};
        const IR::U1 lhs_zero_b{v.ir.FPEqual(lhs_b, zero)};
        const IR::U1 lhs_any_zero{v.ir.LogicalOr(lhs_zero_a, lhs_zero_b)};
        lhs = IR::F16F32F64{v.ir.Select(lhs_any_zero, lhs_c, lhs)};

        const IR::U1 rhs_zero_a{v.ir.FPEqual(rhs_a, zero)};
        const IR::U1 rhs_zero_b{v.ir.FPEqual(rhs_b, zero)};
        const IR::U1 rhs_any_zero{v.ir.LogicalOr(rhs_zero_a, rhs_zero_b)};
        rhs = IR::F16F32F64{v.ir.Select(rhs_any_zero, rhs_c, rhs)};
    }
    if (sat) {
        lhs = v.ir.FPSaturate(lhs);
        rhs = v.ir.FPSaturate(rhs);
    }
    if (promotion) {
        lhs = v.ir.FPConvert(16, lhs);
        rhs = v.ir.FPConvert(16, rhs);
    }
    v.X(hfma2.dest_reg, MergeResult(v.ir, hfma2.dest_reg, lhs, rhs, merge));
}

void HFMA2(TranslatorVisitor& v, u64 insn, bool neg_b, bool neg_c, Swizzle swizzle_b,
           Swizzle swizzle_c, const IR::U32& src_b, const IR::U32& src_c, bool sat,
           HalfPrecision precision) {
    union {
        u64 raw;
        BitField<47, 2, Swizzle> swizzle_a;
        BitField<49, 2, Merge> merge;
    } const hfma2{insn};

    HFMA2(v, insn, hfma2.merge, hfma2.swizzle_a, neg_b, neg_c, swizzle_b, swizzle_c, src_b, src_c,
          sat, precision);
}
} // Anonymous namespace

void TranslatorVisitor::HFMA2_reg(u64 insn) {
    union {
        u64 raw;
        BitField<28, 2, Swizzle> swizzle_b;
        BitField<32, 1, u64> saturate;
        BitField<31, 1, u64> neg_b;
        BitField<30, 1, u64> neg_c;
        BitField<35, 2, Swizzle> swizzle_c;
        BitField<37, 2, HalfPrecision> precision;
    } const hfma2{insn};

    HFMA2(*this, insn, hfma2.neg_b != 0, hfma2.neg_c != 0, hfma2.swizzle_b, hfma2.swizzle_c,
          GetReg20(insn), GetReg39(insn), hfma2.saturate != 0, hfma2.precision);
}

void TranslatorVisitor::HFMA2_rc(u64 insn) {
    union {
        u64 raw;
        BitField<51, 1, u64> neg_c;
        BitField<52, 1, u64> saturate;
        BitField<53, 2, Swizzle> swizzle_b;
        BitField<56, 1, u64> neg_b;
        BitField<57, 2, HalfPrecision> precision;
    } const hfma2{insn};

    HFMA2(*this, insn, hfma2.neg_b != 0, hfma2.neg_c != 0, hfma2.swizzle_b, Swizzle::F32,
          GetReg39(insn), GetCbuf(insn), hfma2.saturate != 0, hfma2.precision);
}

void TranslatorVisitor::HFMA2_cr(u64 insn) {
    union {
        u64 raw;
        BitField<51, 1, u64> neg_c;
        BitField<52, 1, u64> saturate;
        BitField<53, 2, Swizzle> swizzle_c;
        BitField<56, 1, u64> neg_b;
        BitField<57, 2, HalfPrecision> precision;
    } const hfma2{insn};

    HFMA2(*this, insn, hfma2.neg_b != 0, hfma2.neg_c != 0, Swizzle::F32, hfma2.swizzle_c,
          GetCbuf(insn), GetReg39(insn), hfma2.saturate != 0, hfma2.precision);
}

void TranslatorVisitor::HFMA2_imm(u64 insn) {
    union {
        u64 raw;
        BitField<51, 1, u64> neg_c;
        BitField<52, 1, u64> saturate;
        BitField<53, 2, Swizzle> swizzle_c;

        BitField<56, 1, u64> neg_high;
        BitField<30, 9, u64> high;
        BitField<29, 1, u64> neg_low;
        BitField<20, 9, u64> low;
        BitField<57, 2, HalfPrecision> precision;
    } const hfma2{insn};

    const u32 imm{
        static_cast<u32>(hfma2.low << 6) | static_cast<u32>((hfma2.neg_low != 0 ? 1 : 0) << 15) |
        static_cast<u32>(hfma2.high << 22) | static_cast<u32>((hfma2.neg_high != 0 ? 1 : 0) << 31)};

    HFMA2(*this, insn, false, hfma2.neg_c != 0, Swizzle::H1_H0, hfma2.swizzle_c, ir.Imm32(imm),
          GetReg39(insn), hfma2.saturate != 0, hfma2.precision);
}

void TranslatorVisitor::HFMA2_32I(u64 insn) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> src_c;
        BitField<20, 32, u64> imm32;
        BitField<52, 1, u64> neg_c;
        BitField<53, 2, Swizzle> swizzle_a;
        BitField<55, 2, HalfPrecision> precision;
    } const hfma2{insn};

    const u32 imm{static_cast<u32>(hfma2.imm32)};
    HFMA2(*this, insn, Merge::H1_H0, hfma2.swizzle_a, false, hfma2.neg_c != 0, Swizzle::H1_H0,
          Swizzle::H1_H0, ir.Imm32(imm), X(hfma2.src_c), false, hfma2.precision);
}

} // namespace Shader::Maxwell
