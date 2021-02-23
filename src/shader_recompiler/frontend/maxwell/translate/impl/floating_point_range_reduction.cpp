// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class Mode : u64 {
    SINCOS,
    EX2,
};

void RRO(TranslatorVisitor& v, u64 insn, const IR::F32& src) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<39, 1, Mode> mode;
        BitField<45, 1, u64> neg;
        BitField<49, 1, u64> abs;
    } const rro{insn};

    v.F(rro.dest_reg, v.ir.FPAbsNeg(src, rro.abs != 0, rro.neg != 0));
}
} // Anonymous namespace

void TranslatorVisitor::RRO_reg(u64 insn) {
    RRO(*this, insn, GetFloatReg20(insn));
}

void TranslatorVisitor::RRO_cbuf(u64 insn) {
    RRO(*this, insn, GetFloatCbuf(insn));
}

void TranslatorVisitor::RRO_imm(u64) {
    throw NotImplementedException("RRO (imm)");
}

} // namespace Shader::Maxwell
