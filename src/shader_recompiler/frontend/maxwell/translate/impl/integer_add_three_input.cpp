// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class Shift : u64 {
    None,
    Right,
    Left,
};
enum class Half : u64 {
    All,
    Lower,
    Upper,
};

[[nodiscard]] IR::U32 IntegerHalf(IR::IREmitter& ir, const IR::U32& value, Half half) {
    constexpr bool is_signed{false};
    switch (half) {
    case Half::All:
        return value;
    case Half::Lower:
        return ir.BitFieldExtract(value, ir.Imm32(0), ir.Imm32(16), is_signed);
    case Half::Upper:
        return ir.BitFieldExtract(value, ir.Imm32(16), ir.Imm32(16), is_signed);
    }
    throw NotImplementedException("Invalid half");
}

[[nodiscard]] IR::U32 IntegerShift(IR::IREmitter& ir, const IR::U32& value, Shift shift) {
    switch (shift) {
    case Shift::None:
        return value;
    case Shift::Right: {
        // 33-bit RS IADD3 edge case
        const IR::U1 edge_case{ir.GetCarryFromOp(value)};
        const IR::U32 shifted{ir.ShiftRightLogical(value, ir.Imm32(16))};
        return IR::U32{ir.Select(edge_case, ir.IAdd(shifted, ir.Imm32(0x10000)), shifted)};
    }
    case Shift::Left:
        return ir.ShiftLeftLogical(value, ir.Imm32(16));
    }
    throw NotImplementedException("Invalid shift");
}

void IADD3(TranslatorVisitor& v, u64 insn, IR::U32 op_a, IR::U32 op_b, IR::U32 op_c,
           Shift shift = Shift::None) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<47, 1, u64> cc;
        BitField<48, 1, u64> x;
        BitField<49, 1, u64> neg_c;
        BitField<50, 1, u64> neg_b;
        BitField<51, 1, u64> neg_a;
    } iadd3{insn};

    if (iadd3.neg_a != 0) {
        op_a = v.ir.INeg(op_a);
    }
    if (iadd3.neg_b != 0) {
        op_b = v.ir.INeg(op_b);
    }
    if (iadd3.neg_c != 0) {
        op_c = v.ir.INeg(op_c);
    }
    IR::U32 lhs_1{v.ir.IAdd(op_a, op_b)};
    if (iadd3.x != 0) {
        // TODO: How does RS behave when X is set?
        if (shift == Shift::Right) {
            throw NotImplementedException("IADD3 X+RS");
        }
        const IR::U32 carry{v.ir.Select(v.ir.GetCFlag(), v.ir.Imm32(1), v.ir.Imm32(0))};
        lhs_1 = v.ir.IAdd(lhs_1, carry);
    }
    const IR::U32 lhs_2{IntegerShift(v.ir, lhs_1, shift)};
    const IR::U32 result{v.ir.IAdd(lhs_2, op_c)};

    v.X(iadd3.dest_reg, result);
    if (iadd3.cc != 0) {
        // TODO: How does CC behave when X is set?
        if (iadd3.x != 0) {
            throw NotImplementedException("IADD3 X+CC");
        }
        v.SetZFlag(v.ir.GetZeroFromOp(result));
        v.SetSFlag(v.ir.GetSignFromOp(result));
        v.SetCFlag(v.ir.GetCarryFromOp(result));
        const IR::U1 of_1{v.ir.ILessThan(lhs_1, op_a, false)};
        v.SetOFlag(v.ir.LogicalOr(v.ir.GetOverflowFromOp(result), of_1));
    }
}
} // Anonymous namespace

void TranslatorVisitor::IADD3_reg(u64 insn) {
    union {
        u64 insn;
        BitField<37, 2, Shift> shift;
        BitField<35, 2, Half> half_a;
        BitField<33, 2, Half> half_b;
        BitField<31, 2, Half> half_c;
    } const iadd3{insn};

    const auto op_a{IntegerHalf(ir, GetReg8(insn), iadd3.half_a)};
    const auto op_b{IntegerHalf(ir, GetReg20(insn), iadd3.half_b)};
    const auto op_c{IntegerHalf(ir, GetReg39(insn), iadd3.half_c)};
    IADD3(*this, insn, op_a, op_b, op_c, iadd3.shift);
}

void TranslatorVisitor::IADD3_cbuf(u64 insn) {
    IADD3(*this, insn, GetReg8(insn), GetCbuf(insn), GetReg39(insn));
}

void TranslatorVisitor::IADD3_imm(u64 insn) {
    IADD3(*this, insn, GetReg8(insn), GetImm20(insn), GetReg39(insn));
}

} // namespace Shader::Maxwell
