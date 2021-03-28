// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
void ISCADD(TranslatorVisitor& v, u64 insn, IR::U32 op_b) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> op_a;
        BitField<47, 1, u64> cc;
        BitField<48, 2, u64> three_for_po;
        BitField<48, 1, u64> neg_b;
        BitField<49, 1, u64> neg_a;
        BitField<39, 5, u64> scale;
    } const iscadd{insn};

    const bool po{iscadd.three_for_po == 3};
    IR::U32 op_a{v.X(iscadd.op_a)};
    if (!po) {
        // When PO is not present, the bits are interpreted as negation
        if (iscadd.neg_a != 0) {
            op_a = v.ir.INeg(op_a);
        }
        if (iscadd.neg_b != 0) {
            op_b = v.ir.INeg(op_b);
        }
    }
    // With the operands already processed, scale A
    const IR::U32 scale{v.ir.Imm32(static_cast<u32>(iscadd.scale))};
    const IR::U32 scaled_a{v.ir.ShiftLeftLogical(op_a, scale)};

    IR::U32 result{v.ir.IAdd(scaled_a, op_b)};
    if (po) {
        // .PO adds one to the final result
        result = v.ir.IAdd(result, v.ir.Imm32(1));
    }
    v.X(iscadd.dest_reg, result);

    if (iscadd.cc != 0) {
        v.SetZFlag(v.ir.GetZeroFromOp(result));
        v.SetSFlag(v.ir.GetSignFromOp(result));
        v.SetCFlag(v.ir.GetCarryFromOp(result));
        v.SetOFlag(v.ir.GetOverflowFromOp(result));
    }
}

} // Anonymous namespace

void TranslatorVisitor::ISCADD_reg(u64 insn) {
    ISCADD(*this, insn, GetReg20(insn));
}

void TranslatorVisitor::ISCADD_cbuf(u64 insn) {
    ISCADD(*this, insn, GetCbuf(insn));
}

void TranslatorVisitor::ISCADD_imm(u64 insn) {
    ISCADD(*this, insn, GetImm20(insn));
}

void TranslatorVisitor::ISCADD32I(u64) {
    throw NotImplementedException("ISCADD32I");
}

} // namespace Shader::Maxwell
