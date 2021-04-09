// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
void DMNMX(TranslatorVisitor& v, u64 insn, const IR::F64& src_b) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_a_reg;
        BitField<39, 3, IR::Pred> pred;
        BitField<42, 1, u64> neg_pred;
        BitField<45, 1, u64> negate_b;
        BitField<46, 1, u64> abs_a;
        BitField<47, 1, u64> cc;
        BitField<48, 1, u64> negate_a;
        BitField<49, 1, u64> abs_b;
    } const dmnmx{insn};

    if (dmnmx.cc != 0) {
        throw NotImplementedException("DMNMX CC");
    }

    const IR::U1 pred{v.ir.GetPred(dmnmx.pred)};
    const IR::F64 op_a{v.ir.FPAbsNeg(v.D(dmnmx.src_a_reg), dmnmx.abs_a != 0, dmnmx.negate_a != 0)};
    const IR::F64 op_b{v.ir.FPAbsNeg(src_b, dmnmx.abs_b != 0, dmnmx.negate_b != 0)};

    IR::F64 max{v.ir.FPMax(op_a, op_b)};
    IR::F64 min{v.ir.FPMin(op_a, op_b)};

    if (dmnmx.neg_pred != 0) {
        std::swap(min, max);
    }
    v.D(dmnmx.dest_reg, IR::F64{v.ir.Select(pred, min, max)});
}
} // Anonymous namespace

void TranslatorVisitor::DMNMX_reg(u64 insn) {
    DMNMX(*this, insn, GetDoubleReg20(insn));
}

void TranslatorVisitor::DMNMX_cbuf(u64 insn) {
    DMNMX(*this, insn, GetDoubleCbuf(insn));
}

void TranslatorVisitor::DMNMX_imm(u64 insn) {
    DMNMX(*this, insn, GetDoubleImm20(insn));
}

} // namespace Shader::Maxwell
