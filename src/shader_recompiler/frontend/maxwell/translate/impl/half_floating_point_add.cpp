// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/frontend/maxwell/translate/impl/half_floating_point_helper.h"

namespace Shader::Maxwell {
namespace {
void HADD2(TranslatorVisitor& v, u64 insn, Merge merge, bool ftz, bool sat, bool abs_a, bool neg_a,
           Swizzle swizzle_a, bool abs_b, bool neg_b, Swizzle swizzle_b, const IR::U32& src_b) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_a;
    } const hadd2{insn};

    auto [lhs_a, rhs_a]{Extract(v.ir, v.X(hadd2.src_a), swizzle_a)};
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
        .fmz_mode = (ftz ? IR::FmzMode::FTZ : IR::FmzMode::None),
    };
    IR::F16F32F64 lhs{v.ir.FPAdd(lhs_a, lhs_b, fp_control)};
    IR::F16F32F64 rhs{v.ir.FPAdd(rhs_a, rhs_b, fp_control)};
    if (sat) {
        lhs = v.ir.FPSaturate(lhs);
        rhs = v.ir.FPSaturate(rhs);
    }
    if (promotion) {
        lhs = v.ir.FPConvert(16, lhs);
        rhs = v.ir.FPConvert(16, rhs);
    }
    v.X(hadd2.dest_reg, MergeResult(v.ir, hadd2.dest_reg, lhs, rhs, merge));
}

void HADD2(TranslatorVisitor& v, u64 insn, bool sat, bool abs_b, bool neg_b, Swizzle swizzle_b,
           const IR::U32& src_b) {
    union {
        u64 raw;
        BitField<49, 2, Merge> merge;
        BitField<39, 1, u64> ftz;
        BitField<43, 1, u64> neg_a;
        BitField<44, 1, u64> abs_a;
        BitField<47, 2, Swizzle> swizzle_a;
    } const hadd2{insn};

    HADD2(v, insn, hadd2.merge, hadd2.ftz != 0, sat, hadd2.abs_a != 0, hadd2.neg_a != 0,
          hadd2.swizzle_a, abs_b, neg_b, swizzle_b, src_b);
}
} // Anonymous namespace

void TranslatorVisitor::HADD2_reg(u64 insn) {
    union {
        u64 raw;
        BitField<32, 1, u64> sat;
        BitField<31, 1, u64> neg_b;
        BitField<30, 1, u64> abs_b;
        BitField<28, 2, Swizzle> swizzle_b;
    } const hadd2{insn};

    HADD2(*this, insn, hadd2.sat != 0, hadd2.abs_b != 0, hadd2.neg_b != 0, hadd2.swizzle_b,
          GetReg20(insn));
}

void TranslatorVisitor::HADD2_cbuf(u64 insn) {
    union {
        u64 raw;
        BitField<52, 1, u64> sat;
        BitField<56, 1, u64> neg_b;
        BitField<54, 1, u64> abs_b;
    } const hadd2{insn};

    HADD2(*this, insn, hadd2.sat != 0, hadd2.abs_b != 0, hadd2.neg_b != 0, Swizzle::F32,
          GetCbuf(insn));
}

void TranslatorVisitor::HADD2_imm(u64 insn) {
    union {
        u64 raw;
        BitField<52, 1, u64> sat;
        BitField<56, 1, u64> neg_high;
        BitField<30, 9, u64> high;
        BitField<29, 1, u64> neg_low;
        BitField<20, 9, u64> low;
    } const hadd2{insn};

    const u32 imm{
        static_cast<u32>(hadd2.low << 6) | static_cast<u32>((hadd2.neg_low != 0 ? 1 : 0) << 15) |
        static_cast<u32>(hadd2.high << 22) | static_cast<u32>((hadd2.neg_high != 0 ? 1 : 0) << 31)};
    HADD2(*this, insn, hadd2.sat != 0, false, false, Swizzle::H1_H0, ir.Imm32(imm));
}

void TranslatorVisitor::HADD2_32I(u64 insn) {
    union {
        u64 raw;
        BitField<55, 1, u64> ftz;
        BitField<52, 1, u64> sat;
        BitField<56, 1, u64> neg_a;
        BitField<53, 2, Swizzle> swizzle_a;
        BitField<20, 32, u64> imm32;
    } const hadd2{insn};

    const u32 imm{static_cast<u32>(hadd2.imm32)};
    HADD2(*this, insn, Merge::H1_H0, hadd2.ftz != 0, hadd2.sat != 0, false, hadd2.neg_a != 0,
          hadd2.swizzle_a, false, false, Swizzle::H1_H0, ir.Imm32(imm));
}
} // namespace Shader::Maxwell
