// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/common_encoding.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
void FFMA(TranslatorVisitor& v, u64 insn, const IR::F32& src_b, const IR::F32& src_c, bool neg_a,
          bool neg_b, bool neg_c, bool sat, bool cc, FmzMode fmz_mode, FpRounding fp_rounding) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_a;
    } const ffma{insn};

    if (cc) {
        throw NotImplementedException("FFMA CC");
    }
    const IR::F32 op_a{v.ir.FPAbsNeg(v.F(ffma.src_a), false, neg_a)};
    const IR::F32 op_b{v.ir.FPAbsNeg(src_b, false, neg_b)};
    const IR::F32 op_c{v.ir.FPAbsNeg(src_c, false, neg_c)};
    const IR::FpControl fp_control{
        .no_contraction = true,
        .rounding = CastFpRounding(fp_rounding),
        .fmz_mode = CastFmzMode(fmz_mode),
    };
    IR::F32 value{v.ir.FPFma(op_a, op_b, op_c, fp_control)};
    if (fmz_mode == FmzMode::FMZ && !sat) {
        // Do not implement FMZ if SAT is enabled, as it does the logic for us.
        // On D3D9 mode, anything * 0 is zero, even NAN and infinity
        const IR::F32 zero{v.ir.Imm32(0.0f)};
        const IR::U1 zero_a{v.ir.FPEqual(op_a, zero)};
        const IR::U1 zero_b{v.ir.FPEqual(op_b, zero)};
        const IR::U1 any_zero{v.ir.LogicalOr(zero_a, zero_b)};
        value = IR::F32{v.ir.Select(any_zero, op_c, value)};
    }
    if (sat) {
        value = v.ir.FPSaturate(value);
    }
    v.F(ffma.dest_reg, value);
}

void FFMA(TranslatorVisitor& v, u64 insn, const IR::F32& src_b, const IR::F32& src_c) {
    union {
        u64 raw;
        BitField<47, 1, u64> cc;
        BitField<48, 1, u64> neg_b;
        BitField<49, 1, u64> neg_c;
        BitField<50, 1, u64> sat;
        BitField<51, 2, FpRounding> fp_rounding;
        BitField<53, 2, FmzMode> fmz_mode;
    } const ffma{insn};

    FFMA(v, insn, src_b, src_c, false, ffma.neg_b != 0, ffma.neg_c != 0, ffma.sat != 0,
         ffma.cc != 0, ffma.fmz_mode, ffma.fp_rounding);
}
} // Anonymous namespace

void TranslatorVisitor::FFMA_reg(u64 insn) {
    FFMA(*this, insn, GetFloatReg20(insn), GetFloatReg39(insn));
}

void TranslatorVisitor::FFMA_rc(u64 insn) {
    FFMA(*this, insn, GetFloatReg39(insn), GetFloatCbuf(insn));
}

void TranslatorVisitor::FFMA_cr(u64 insn) {
    FFMA(*this, insn, GetFloatCbuf(insn), GetFloatReg39(insn));
}

void TranslatorVisitor::FFMA_imm(u64 insn) {
    FFMA(*this, insn, GetFloatImm20(insn), GetFloatReg39(insn));
}

void TranslatorVisitor::FFMA32I(u64 insn) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> src_c; // FFMA32I mirrors the destination and addition register
        BitField<52, 1, u64> cc;
        BitField<53, 2, FmzMode> fmz_mode;
        BitField<55, 1, u64> sat;
        BitField<56, 1, u64> neg_a;
        BitField<57, 1, u64> neg_c;
    } const ffma32i{insn};

    FFMA(*this, insn, GetFloatImm32(insn), F(ffma32i.src_c), ffma32i.neg_a != 0, false,
         ffma32i.neg_c != 0, ffma32i.sat != 0, ffma32i.cc != 0, ffma32i.fmz_mode, FpRounding::RN);
}

} // namespace Shader::Maxwell
