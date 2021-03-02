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
    case Half::Lower:
        return ir.BitFieldExtract(value, ir.Imm32(0), ir.Imm32(16), is_signed);
    case Half::Upper:
        return ir.BitFieldExtract(value, ir.Imm32(16), ir.Imm32(16), is_signed);
    default:
        return value;
    }
}

[[nodiscard]] IR::U32 IntegerShift(IR::IREmitter& ir, const IR::U32& value, Shift shift) {
    switch (shift) {
    case Shift::Right:
        return ir.ShiftRightLogical(value, ir.Imm32(16));
    case Shift::Left:
        return ir.ShiftLeftLogical(value, ir.Imm32(16));
    default:
        return value;
    }
}

void IADD3(TranslatorVisitor& v, u64 insn, IR::U32 op_b, IR::U32 op_c) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_a;
        BitField<31, 2, Half> half_c;
        BitField<33, 2, Half> half_b;
        BitField<35, 2, Half> half_a;
        BitField<37, 2, Shift> shift;
        BitField<47, 1, u64> cc;
        BitField<48, 1, u64> x;
        BitField<49, 1, u64> neg_c;
        BitField<50, 1, u64> neg_b;
        BitField<51, 1, u64> neg_a;
    } iadd3{insn};

    if (iadd3.x != 0) {
        throw NotImplementedException("IADD3 X");
    }
    if (iadd3.cc != 0) {
        throw NotImplementedException("IADD3 CC");
    }

    IR::U32 op_a{v.X(iadd3.src_a)};
    op_a = IntegerHalf(v.ir, op_a, iadd3.half_a);
    op_b = IntegerHalf(v.ir, op_b, iadd3.half_b);
    op_c = IntegerHalf(v.ir, op_c, iadd3.half_c);

    if (iadd3.neg_a != 0) {
        op_a = v.ir.INeg(op_a);
    }
    if (iadd3.neg_b != 0) {
        op_b = v.ir.INeg(op_b);
    }
    if (iadd3.neg_c != 0) {
        op_c = v.ir.INeg(op_c);
    }

    IR::U32 lhs{v.ir.IAdd(op_a, op_b)};
    lhs = IntegerShift(v.ir, lhs, iadd3.shift);
    const IR::U32 result{v.ir.IAdd(lhs, op_c)};

    v.X(iadd3.dest_reg, result);
}
} // Anonymous namespace

void TranslatorVisitor::IADD3_reg(u64 insn) {
    IADD3(*this, insn, GetReg20(insn), GetReg39(insn));
}

void TranslatorVisitor::IADD3_cbuf(u64 insn) {
    IADD3(*this, insn, GetCbuf(insn), GetReg39(insn));
}

void TranslatorVisitor::IADD3_imm(u64 insn) {
    IADD3(*this, insn, GetImm20(insn), GetReg39(insn));
}

} // namespace Shader::Maxwell
