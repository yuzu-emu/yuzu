// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
void IMNMX(TranslatorVisitor& v, u64 insn, const IR::U32& op_b) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_reg;
        BitField<39, 3, IR::Pred> pred;
        BitField<42, 1, u64> neg_pred;
        BitField<43, 2, u64> mode;
        BitField<47, 1, u64> cc;
        BitField<48, 1, u64> is_signed;
    } const imnmx{insn};

    if (imnmx.cc != 0) {
        throw NotImplementedException("IMNMX CC");
    }

    if (imnmx.mode != 0) {
        throw NotImplementedException("IMNMX.MODE");
    }

    const IR::U1 pred{v.ir.GetPred(imnmx.pred)};
    const IR::U32 op_a{v.X(imnmx.src_reg)};
    IR::U32 min;
    IR::U32 max;

    if (imnmx.is_signed != 0) {
        min = IR::U32{v.ir.SMin(op_a, op_b)};
        max = IR::U32{v.ir.SMax(op_a, op_b)};
    } else {
        min = IR::U32{v.ir.UMin(op_a, op_b)};
        max = IR::U32{v.ir.UMax(op_a, op_b)};
    }
    if (imnmx.neg_pred != 0) {
        std::swap(min, max);
    }

    const IR::U32 result{v.ir.Select(pred, min, max)};
    v.X(imnmx.dest_reg, result);
}
} // Anonymous namespace

void TranslatorVisitor::IMNMX_reg(u64 insn) {
    IMNMX(*this, insn, GetReg20(insn));
}

void TranslatorVisitor::IMNMX_cbuf(u64 insn) {
    IMNMX(*this, insn, GetCbuf(insn));
}

void TranslatorVisitor::IMNMX_imm(u64 insn) {
    IMNMX(*this, insn, GetImm20(insn));
}

} // namespace Shader::Maxwell
