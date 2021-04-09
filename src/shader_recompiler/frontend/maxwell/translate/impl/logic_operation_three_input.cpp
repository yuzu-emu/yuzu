// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/common_funcs.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
// https://forums.developer.nvidia.com/t/reverse-lut-for-lop3-lut/110651
// Emulate GPU's LOP3.LUT (three-input logic op with 8-bit truth table)
IR::U32 ApplyLUT(IR::IREmitter& ir, const IR::U32& a, const IR::U32& b, const IR::U32& c,
                 u64 ttbl) {
    IR::U32 r{ir.Imm32(0)};
    const IR::U32 not_a{ir.BitwiseNot(a)};
    const IR::U32 not_b{ir.BitwiseNot(b)};
    const IR::U32 not_c{ir.BitwiseNot(c)};
    if (ttbl & 0x01) {
        // r |= ~a & ~b & ~c;
        const auto lhs{ir.BitwiseAnd(not_a, not_b)};
        const auto rhs{ir.BitwiseAnd(lhs, not_c)};
        r = ir.BitwiseOr(r, rhs);
    }
    if (ttbl & 0x02) {
        // r |= ~a & ~b & c;
        const auto lhs{ir.BitwiseAnd(not_a, not_b)};
        const auto rhs{ir.BitwiseAnd(lhs, c)};
        r = ir.BitwiseOr(r, rhs);
    }
    if (ttbl & 0x04) {
        // r |= ~a & b & ~c;
        const auto lhs{ir.BitwiseAnd(not_a, b)};
        const auto rhs{ir.BitwiseAnd(lhs, not_c)};
        r = ir.BitwiseOr(r, rhs);
    }
    if (ttbl & 0x08) {
        // r |= ~a & b & c;
        const auto lhs{ir.BitwiseAnd(not_a, b)};
        const auto rhs{ir.BitwiseAnd(lhs, c)};
        r = ir.BitwiseOr(r, rhs);
    }
    if (ttbl & 0x10) {
        // r |= a & ~b & ~c;
        const auto lhs{ir.BitwiseAnd(a, not_b)};
        const auto rhs{ir.BitwiseAnd(lhs, not_c)};
        r = ir.BitwiseOr(r, rhs);
    }
    if (ttbl & 0x20) {
        // r |= a & ~b & c;
        const auto lhs{ir.BitwiseAnd(a, not_b)};
        const auto rhs{ir.BitwiseAnd(lhs, c)};
        r = ir.BitwiseOr(r, rhs);
    }
    if (ttbl & 0x40) {
        // r |= a & b & ~c;
        const auto lhs{ir.BitwiseAnd(a, b)};
        const auto rhs{ir.BitwiseAnd(lhs, not_c)};
        r = ir.BitwiseOr(r, rhs);
    }
    if (ttbl & 0x80) {
        // r |= a & b & c;
        const auto lhs{ir.BitwiseAnd(a, b)};
        const auto rhs{ir.BitwiseAnd(lhs, c)};
        r = ir.BitwiseOr(r, rhs);
    }
    return r;
}

IR::U32 LOP3(TranslatorVisitor& v, u64 insn, const IR::U32& op_b, const IR::U32& op_c, u64 lut) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_reg;
        BitField<47, 1, u64> cc;
    } const lop3{insn};

    if (lop3.cc != 0) {
        throw NotImplementedException("LOP3 CC");
    }

    const IR::U32 op_a{v.X(lop3.src_reg)};
    const IR::U32 result{ApplyLUT(v.ir, op_a, op_b, op_c, lut)};
    v.X(lop3.dest_reg, result);
    return result;
}

u64 GetLut48(u64 insn) {
    union {
        u64 raw;
        BitField<48, 8, u64> lut;
    } const lut{insn};
    return lut.lut;
}
} // Anonymous namespace

void TranslatorVisitor::LOP3_reg(u64 insn) {
    union {
        u64 insn;
        BitField<28, 8, u64> lut;
        BitField<38, 1, u64> x;
        BitField<36, 2, PredicateOp> pred_op;
        BitField<48, 3, IR::Pred> pred;
    } const lop3{insn};

    if (lop3.x != 0) {
        throw NotImplementedException("LOP3 X");
    }
    const IR::U32 result{LOP3(*this, insn, GetReg20(insn), GetReg39(insn), lop3.lut)};
    const IR::U1 pred_result{PredicateOperation(ir, result, lop3.pred_op)};
    ir.SetPred(lop3.pred, pred_result);
}

void TranslatorVisitor::LOP3_cbuf(u64 insn) {
    LOP3(*this, insn, GetCbuf(insn), GetReg39(insn), GetLut48(insn));
}

void TranslatorVisitor::LOP3_imm(u64 insn) {
    LOP3(*this, insn, GetImm20(insn), GetReg39(insn), GetLut48(insn));
}
} // namespace Shader::Maxwell
