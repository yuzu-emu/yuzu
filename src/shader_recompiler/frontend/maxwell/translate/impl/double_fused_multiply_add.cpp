// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/common_encoding.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {

void DFMA(TranslatorVisitor& v, u64 insn, const IR::F64& src_b, const IR::F64& src_c) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_a_reg;
        BitField<50, 2, FpRounding> fp_rounding;
        BitField<47, 1, u64> cc;
        BitField<48, 1, u64> neg_b;
        BitField<49, 1, u64> neg_c;
    } const dfma{insn};

    if (dfma.cc != 0) {
        throw NotImplementedException("DFMA CC");
    }

    const IR::F64 src_a{v.D(dfma.src_a_reg)};
    const IR::F64 op_b{v.ir.FPAbsNeg(src_b, false, dfma.neg_b != 0)};
    const IR::F64 op_c{v.ir.FPAbsNeg(src_c, false, dfma.neg_c != 0)};

    const IR::FpControl control{
        .no_contraction = true,
        .rounding = CastFpRounding(dfma.fp_rounding),
        .fmz_mode = IR::FmzMode::None,
    };

    v.D(dfma.dest_reg, v.ir.FPFma(src_a, op_b, op_c, control));
}
} // Anonymous namespace

void TranslatorVisitor::DFMA_reg(u64 insn) {
    DFMA(*this, insn, GetDoubleReg20(insn), GetDoubleReg39(insn));
}

void TranslatorVisitor::DFMA_cr(u64 insn) {
    DFMA(*this, insn, GetDoubleCbuf(insn), GetDoubleReg39(insn));
}

void TranslatorVisitor::DFMA_rc(u64 insn) {
    DFMA(*this, insn, GetDoubleReg39(insn), GetDoubleCbuf(insn));
}

void TranslatorVisitor::DFMA_imm(u64 insn) {
    DFMA(*this, insn, GetDoubleImm20(insn), GetDoubleReg39(insn));
}

} // namespace Shader::Maxwell
