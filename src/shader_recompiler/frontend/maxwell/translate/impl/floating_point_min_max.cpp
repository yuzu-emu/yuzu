// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
void FMNMX(TranslatorVisitor& v, u64 insn, const IR::F32& src_b) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_a_reg;
        BitField<39, 3, IR::Pred> pred;
        BitField<42, 1, u64> neg_pred;
        BitField<44, 1, u64> ftz;
        BitField<45, 1, u64> negate_b;
        BitField<46, 1, u64> abs_a;
        BitField<47, 1, u64> cc;
        BitField<48, 1, u64> negate_a;
        BitField<49, 1, u64> abs_b;
    } const fmnmx{insn};

    if (fmnmx.cc) {
        throw NotImplementedException("FMNMX CC");
    }

    const IR::U1 pred{v.ir.GetPred(fmnmx.pred)};
    const IR::F32 op_a{v.ir.FPAbsNeg(v.F(fmnmx.src_a_reg), fmnmx.abs_a != 0, fmnmx.negate_a != 0)};
    const IR::F32 op_b{v.ir.FPAbsNeg(src_b, fmnmx.abs_b != 0, fmnmx.negate_b != 0)};

    const IR::FpControl control{
        .no_contraction = false,
        .rounding = IR::FpRounding::DontCare,
        .fmz_mode = (fmnmx.ftz != 0 ? IR::FmzMode::FTZ : IR::FmzMode::None),
    };
    IR::F32 max{v.ir.FPMax(op_a, op_b, control)};
    IR::F32 min{v.ir.FPMin(op_a, op_b, control)};

    if (fmnmx.neg_pred != 0) {
        std::swap(min, max);
    }

    v.F(fmnmx.dest_reg, IR::F32{v.ir.Select(pred, min, max)});
}
} // Anonymous namespace

void TranslatorVisitor::FMNMX_reg(u64 insn) {
    FMNMX(*this, insn, GetFloatReg20(insn));
}

void TranslatorVisitor::FMNMX_cbuf(u64 insn) {
    FMNMX(*this, insn, GetFloatCbuf(insn));
}

void TranslatorVisitor::FMNMX_imm(u64 insn) {
    FMNMX(*this, insn, GetFloatImm20(insn));
}

} // namespace Shader::Maxwell
