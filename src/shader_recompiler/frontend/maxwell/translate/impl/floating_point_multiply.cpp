// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/frontend/ir/modifiers.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/common_encoding.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class Scale : u64 {
    None,
    D2,
    D4,
    D8,
    M8,
    M4,
    M2,
    INVALIDSCALE37,
};

float ScaleFactor(Scale scale) {
    switch (scale) {
    case Scale::None:
        return 1.0f;
    case Scale::D2:
        return 1.0f / 2.0f;
    case Scale::D4:
        return 1.0f / 4.0f;
    case Scale::D8:
        return 1.0f / 8.0f;
    case Scale::M8:
        return 8.0f;
    case Scale::M4:
        return 4.0f;
    case Scale::M2:
        return 2.0f;
    case Scale::INVALIDSCALE37:
        break;
    }
    throw NotImplementedException("Invalid FMUL scale {}", scale);
}

void FMUL(TranslatorVisitor& v, u64 insn, const IR::F32& src_b, FmzMode fmz_mode,
          FpRounding fp_rounding, Scale scale, bool sat, bool cc, bool neg_b) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_a;
    } const fmul{insn};

    if (cc) {
        throw NotImplementedException("FMUL CC");
    }
    IR::F32 op_a{v.F(fmul.src_a)};
    if (scale != Scale::None) {
        if (fmz_mode != FmzMode::FTZ || fp_rounding != FpRounding::RN) {
            throw NotImplementedException("FMUL scale with non-FMZ or non-RN modifiers");
        }
        op_a = v.ir.FPMul(op_a, v.ir.Imm32(ScaleFactor(scale)));
    }
    const IR::F32 op_b{v.ir.FPAbsNeg(src_b, false, neg_b)};
    const IR::FpControl fp_control{
        .no_contraction = true,
        .rounding = CastFpRounding(fp_rounding),
        .fmz_mode = CastFmzMode(fmz_mode),
    };
    IR::F32 value{v.ir.FPMul(op_a, op_b, fp_control)};
    if (fmz_mode == FmzMode::FMZ && !sat) {
        // Do not implement FMZ if SAT is enabled, as it does the logic for us.
        // On D3D9 mode, anything * 0 is zero, even NAN and infinity
        const IR::F32 zero{v.ir.Imm32(0.0f)};
        const IR::U1 zero_a{v.ir.FPEqual(op_a, zero)};
        const IR::U1 zero_b{v.ir.FPEqual(op_b, zero)};
        const IR::U1 any_zero{v.ir.LogicalOr(zero_a, zero_b)};
        value = IR::F32{v.ir.Select(any_zero, zero, value)};
    }
    if (sat) {
        value = v.ir.FPSaturate(value);
    }
    v.F(fmul.dest_reg, value);
}

void FMUL(TranslatorVisitor& v, u64 insn, const IR::F32& src_b) {
    union {
        u64 raw;
        BitField<39, 2, FpRounding> fp_rounding;
        BitField<41, 3, Scale> scale;
        BitField<44, 2, FmzMode> fmz;
        BitField<47, 1, u64> cc;
        BitField<48, 1, u64> neg_b;
        BitField<50, 1, u64> sat;
    } const fmul{insn};

    FMUL(v, insn, src_b, fmul.fmz, fmul.fp_rounding, fmul.scale, fmul.sat != 0, fmul.cc != 0,
         fmul.neg_b != 0);
}
} // Anonymous namespace

void TranslatorVisitor::FMUL_reg(u64 insn) {
    return FMUL(*this, insn, GetFloatReg20(insn));
}

void TranslatorVisitor::FMUL_cbuf(u64 insn) {
    return FMUL(*this, insn, GetFloatCbuf(insn));
}

void TranslatorVisitor::FMUL_imm(u64 insn) {
    return FMUL(*this, insn, GetFloatImm20(insn));
}

void TranslatorVisitor::FMUL32I(u64 insn) {
    union {
        u64 raw;
        BitField<52, 1, u64> cc;
        BitField<53, 2, FmzMode> fmz;
        BitField<55, 1, u64> sat;
    } const fmul32i{insn};

    FMUL(*this, insn, GetFloatImm32(insn), fmul32i.fmz, FpRounding::RN, Scale::None,
         fmul32i.sat != 0, fmul32i.cc != 0, false);
}

} // namespace Shader::Maxwell
