// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
void ISCADD(TranslatorVisitor& v, u64 insn, IR::U32 op_b, bool cc, bool neg_a, bool neg_b,
            u64 scale_imm) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> op_a;
    } const iscadd{insn};

    const bool po{neg_a && neg_b};
    IR::U32 op_a{v.X(iscadd.op_a)};
    if (po) {
        // When PO is present, add one
        op_b = v.ir.IAdd(op_b, v.ir.Imm32(1));
    } else {
        // When PO is not present, the bits are interpreted as negation
        if (neg_a) {
            op_a = v.ir.INeg(op_a);
        }
        if (neg_b) {
            op_b = v.ir.INeg(op_b);
        }
    }
    // With the operands already processed, scale A
    const IR::U32 scale{v.ir.Imm32(static_cast<u32>(scale_imm))};
    const IR::U32 scaled_a{v.ir.ShiftLeftLogical(op_a, scale)};

    const IR::U32 result{v.ir.IAdd(scaled_a, op_b)};
    v.X(iscadd.dest_reg, result);

    if (cc) {
        v.SetZFlag(v.ir.GetZeroFromOp(result));
        v.SetSFlag(v.ir.GetSignFromOp(result));
        const IR::U1 carry{v.ir.GetCarryFromOp(result)};
        const IR::U1 overflow{v.ir.GetOverflowFromOp(result)};
        v.SetCFlag(po ? v.ir.LogicalOr(carry, v.ir.GetCarryFromOp(op_b)) : carry);
        v.SetOFlag(po ? v.ir.LogicalOr(overflow, v.ir.GetOverflowFromOp(op_b)) : overflow);
    }
}

void ISCADD(TranslatorVisitor& v, u64 insn, IR::U32 op_b) {
    union {
        u64 raw;
        BitField<47, 1, u64> cc;
        BitField<48, 1, u64> neg_b;
        BitField<49, 1, u64> neg_a;
        BitField<39, 5, u64> scale;
    } const iscadd{insn};

    ISCADD(v, insn, op_b, iscadd.cc != 0, iscadd.neg_a != 0, iscadd.neg_b != 0, iscadd.scale);
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

void TranslatorVisitor::ISCADD32I(u64 insn) {
    union {
        u64 raw;
        BitField<52, 1, u64> cc;
        BitField<53, 5, u64> scale;
    } const iscadd{insn};

    return ISCADD(*this, insn, GetImm32(insn), iscadd.cc != 0, false, false, iscadd.scale);
}

} // namespace Shader::Maxwell
