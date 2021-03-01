// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class ComparisonOp : u64 {
    False,
    LessThan,
    Equal,
    LessThanEqual,
    GreaterThan,
    NotEqual,
    GreaterThanEqual,
    True,
};

[[nodiscard]] IR::U1 CompareToZero(TranslatorVisitor& v, const IR::U32& operand,
                                   ComparisonOp compare_op, bool is_signed) {
    const IR::U32 zero{v.ir.Imm32(0)};
    switch (compare_op) {
    case ComparisonOp::False:
        return v.ir.Imm1(false);
    case ComparisonOp::LessThan:
        return v.ir.ILessThan(operand, zero, is_signed);
    case ComparisonOp::Equal:
        return v.ir.IEqual(operand, zero);
    case ComparisonOp::LessThanEqual:
        return v.ir.ILessThanEqual(operand, zero, is_signed);
    case ComparisonOp::GreaterThan:
        return v.ir.IGreaterThan(operand, zero, is_signed);
    case ComparisonOp::NotEqual:
        return v.ir.INotEqual(operand, zero);
    case ComparisonOp::GreaterThanEqual:
        return v.ir.IGreaterThanEqual(operand, zero, is_signed);
    case ComparisonOp::True:
        return v.ir.Imm1(true);
    default:
        throw NotImplementedException("ICMP.CMP");
    }
}

void ICMP(TranslatorVisitor& v, u64 insn, const IR::U32& src_a, const IR::U32& operand) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_reg;
        BitField<48, 1, u64> is_signed;
        BitField<49, 3, ComparisonOp> compare_op;
    } const icmp{insn};

    const IR::U32 zero{v.ir.Imm32(0)};
    const bool is_signed{icmp.is_signed != 0};
    const IR::U1 cmp_result{CompareToZero(v, operand, icmp.compare_op, is_signed)};

    const IR::U32 src_reg{v.X(icmp.src_reg)};
    const IR::U32 result{v.ir.Select(cmp_result, src_reg, src_a)};

    v.X(icmp.dest_reg, result);
}
} // Anonymous namespace

void TranslatorVisitor::ICMP_reg(u64 insn) {
    ICMP(*this, insn, GetReg20(insn), GetReg39(insn));
}

void TranslatorVisitor::ICMP_rc(u64 insn) {
    ICMP(*this, insn, GetReg39(insn), GetCbuf(insn));
}

void TranslatorVisitor::ICMP_cr(u64 insn) {
    ICMP(*this, insn, GetCbuf(insn), GetReg39(insn));
}

void TranslatorVisitor::ICMP_imm(u64 insn) {
    ICMP(*this, insn, GetImm20(insn), GetReg39(insn));
}

} // namespace Shader::Maxwell
