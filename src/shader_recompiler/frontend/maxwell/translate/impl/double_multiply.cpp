// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/common_encoding.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {

void DMUL(TranslatorVisitor& v, u64 insn, const IR::F64& src_b) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_a_reg;
        BitField<39, 2, FpRounding> fp_rounding;
        BitField<47, 1, u64> cc;
        BitField<48, 1, u64> neg;
    } const dmul{insn};

    if (dmul.cc != 0) {
        throw NotImplementedException("DMUL CC");
    }

    const IR::F64 src_a{v.ir.FPAbsNeg(v.D(dmul.src_a_reg), false, dmul.neg != 0)};
    const IR::FpControl control{
        .no_contraction = true,
        .rounding = CastFpRounding(dmul.fp_rounding),
        .fmz_mode = IR::FmzMode::None,
    };

    v.D(dmul.dest_reg, v.ir.FPMul(src_a, src_b, control));
}
} // Anonymous namespace

void TranslatorVisitor::DMUL_reg(u64 insn) {
    DMUL(*this, insn, GetDoubleReg20(insn));
}

void TranslatorVisitor::DMUL_cbuf(u64 insn) {
    DMUL(*this, insn, GetDoubleCbuf(insn));
}

void TranslatorVisitor::DMUL_imm(u64 insn) {
    DMUL(*this, insn, GetDoubleImm20(insn));
}

} // namespace Shader::Maxwell
