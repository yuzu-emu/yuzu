// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/common_funcs.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
void ISET(TranslatorVisitor& v, u64 insn, const IR::U32& src_a) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_reg;
        BitField<39, 3, IR::Pred> pred;
        BitField<42, 1, u64> neg_pred;
        BitField<43, 1, u64> x;
        BitField<44, 1, u64> bf;
        BitField<45, 2, BooleanOp> bop;
        BitField<48, 1, u64> is_signed;
        BitField<49, 3, ComparisonOp> compare_op;
    } const iset{insn};

    if (iset.x != 0) {
        throw NotImplementedException("ISET.X");
    }

    const IR::U32 src_reg{v.X(iset.src_reg)};
    const bool is_signed{iset.is_signed != 0};
    IR::U1 pred{v.ir.GetPred(iset.pred)};
    if (iset.neg_pred != 0) {
        pred = v.ir.LogicalNot(pred);
    }
    const IR::U1 cmp_result{IntegerCompare(v, src_reg, src_a, iset.compare_op, is_signed)};
    const IR::U1 bop_result{PredicateCombine(v, cmp_result, pred, iset.bop)};

    const IR::U32 one_mask{v.ir.Imm32(-1)};
    const IR::U32 fp_one{v.ir.Imm32(0x3f800000)};
    const IR::U32 fail_result{v.ir.Imm32(0)};
    const IR::U32 pass_result{iset.bf == 0 ? one_mask : fp_one};

    const IR::U32 result{v.ir.Select(bop_result, pass_result, fail_result)};

    v.X(iset.dest_reg, result);
}
} // Anonymous namespace

void TranslatorVisitor::ISET_reg(u64 insn) {
    ISET(*this, insn, GetReg20(insn));
}

void TranslatorVisitor::ISET_cbuf(u64 insn) {
    ISET(*this, insn, GetCbuf(insn));
}

void TranslatorVisitor::ISET_imm(u64 insn) {
    ISET(*this, insn, GetImm20(insn));
}

} // namespace Shader::Maxwell
