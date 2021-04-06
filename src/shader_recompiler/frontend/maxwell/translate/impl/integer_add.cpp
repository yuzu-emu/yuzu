// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
void IADD(TranslatorVisitor& v, u64 insn, const IR::U32 op_b, bool neg_a, bool po, bool sat, bool x,
          bool cc) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_a;
    } const iadd{insn};

    if (sat) {
        throw NotImplementedException("IADD SAT");
    }
    if (x && po) {
        throw NotImplementedException("IADD X+PO");
    }
    // Operand A is always read from here, negated if needed
    IR::U32 op_a{v.X(iadd.src_a)};
    if (neg_a) {
        op_a = v.ir.INeg(op_a);
    }
    // Add both operands
    IR::U32 result{v.ir.IAdd(op_a, op_b)};
    if (x) {
        const IR::U32 carry{v.ir.Select(v.ir.GetCFlag(), v.ir.Imm32(1), v.ir.Imm32(0))};
        result = v.ir.IAdd(result, carry);
    }
    if (po) {
        // .PO adds one to the result
        result = v.ir.IAdd(result, v.ir.Imm32(1));
    }
    if (cc) {
        // Store flags
        // TODO: Does this grab the result pre-PO or after?
        if (po) {
            throw NotImplementedException("IADD CC+PO");
        }
        // TODO: How does CC behave when X is set?
        if (x) {
            throw NotImplementedException("IADD X+CC");
        }
        v.SetZFlag(v.ir.GetZeroFromOp(result));
        v.SetSFlag(v.ir.GetSignFromOp(result));
        v.SetCFlag(v.ir.GetCarryFromOp(result));
        v.SetOFlag(v.ir.GetOverflowFromOp(result));
    }
    // Store result
    v.X(iadd.dest_reg, result);
}

void IADD(TranslatorVisitor& v, u64 insn, IR::U32 op_b) {
    union {
        u64 insn;
        BitField<43, 1, u64> x;
        BitField<47, 1, u64> cc;
        BitField<48, 2, u64> three_for_po;
        BitField<48, 1, u64> neg_b;
        BitField<49, 1, u64> neg_a;
        BitField<50, 1, u64> sat;
    } const iadd{insn};

    const bool po{iadd.three_for_po == 3};
    if (!po && iadd.neg_b != 0) {
        op_b = v.ir.INeg(op_b);
    }
    IADD(v, insn, op_b, iadd.neg_a != 0, po, iadd.sat != 0, iadd.x != 0, iadd.cc != 0);
}
} // Anonymous namespace

void TranslatorVisitor::IADD_reg(u64 insn) {
    IADD(*this, insn, GetReg20(insn));
}

void TranslatorVisitor::IADD_cbuf(u64 insn) {
    IADD(*this, insn, GetCbuf(insn));
}

void TranslatorVisitor::IADD_imm(u64 insn) {
    IADD(*this, insn, GetImm20(insn));
}

void TranslatorVisitor::IADD32I(u64 insn) {
    union {
        u64 raw;
        BitField<52, 1, u64> cc;
        BitField<53, 1, u64> x;
        BitField<54, 1, u64> sat;
        BitField<55, 2, u64> three_for_po;
        BitField<56, 1, u64> neg_a;
    } const iadd32i{insn};

    const bool po{iadd32i.three_for_po == 3};
    const bool neg_a{!po && iadd32i.neg_a != 0};
    IADD(*this, insn, GetImm32(insn), neg_a, po, iadd32i.sat != 0, iadd32i.x != 0, iadd32i.cc != 0);
}

} // namespace Shader::Maxwell
