// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/frontend/maxwell/translate/impl/common_funcs.h"

namespace Shader::Maxwell {
IR::U1 IntegerCompare(IR::IREmitter& ir, const IR::U32& operand_1, const IR::U32& operand_2,
                      CompareOp compare_op, bool is_signed) {
    switch (compare_op) {
    case CompareOp::False:
        return ir.Imm1(false);
    case CompareOp::LessThan:
        return ir.ILessThan(operand_1, operand_2, is_signed);
    case CompareOp::Equal:
        return ir.IEqual(operand_1, operand_2);
    case CompareOp::LessThanEqual:
        return ir.ILessThanEqual(operand_1, operand_2, is_signed);
    case CompareOp::GreaterThan:
        return ir.IGreaterThan(operand_1, operand_2, is_signed);
    case CompareOp::NotEqual:
        return ir.INotEqual(operand_1, operand_2);
    case CompareOp::GreaterThanEqual:
        return ir.IGreaterThanEqual(operand_1, operand_2, is_signed);
    case CompareOp::True:
        return ir.Imm1(true);
    default:
        throw NotImplementedException("Invalid compare op {}", compare_op);
    }
}

IR::U1 PredicateCombine(IR::IREmitter& ir, const IR::U1& predicate_1, const IR::U1& predicate_2,
                        BooleanOp bop) {
    switch (bop) {
    case BooleanOp::AND:
        return ir.LogicalAnd(predicate_1, predicate_2);
    case BooleanOp::OR:
        return ir.LogicalOr(predicate_1, predicate_2);
    case BooleanOp::XOR:
        return ir.LogicalXor(predicate_1, predicate_2);
    default:
        throw NotImplementedException("Invalid bop {}", bop);
    }
}

IR::U1 PredicateOperation(IR::IREmitter& ir, const IR::U32& result, PredicateOp op) {
    switch (op) {
    case PredicateOp::False:
        return ir.Imm1(false);
    case PredicateOp::True:
        return ir.Imm1(true);
    case PredicateOp::Zero:
        return ir.IEqual(result, ir.Imm32(0));
    case PredicateOp::NonZero:
        return ir.INotEqual(result, ir.Imm32(0));
    default:
        throw NotImplementedException("Invalid Predicate operation {}", op);
    }
}

bool IsCompareOpOrdered(FPCompareOp op) {
    switch (op) {
    case FPCompareOp::LTU:
    case FPCompareOp::EQU:
    case FPCompareOp::LEU:
    case FPCompareOp::GTU:
    case FPCompareOp::NEU:
    case FPCompareOp::GEU:
        return false;
    default:
        return true;
    }
}

IR::U1 FloatingPointCompare(IR::IREmitter& ir, const IR::F16F32F64& operand_1,
                            const IR::F16F32F64& operand_2, FPCompareOp compare_op,
                            IR::FpControl control) {
    const bool ordered{IsCompareOpOrdered(compare_op)};
    switch (compare_op) {
    case FPCompareOp::F:
        return ir.Imm1(false);
    case FPCompareOp::LT:
    case FPCompareOp::LTU:
        return ir.FPLessThan(operand_1, operand_2, control, ordered);
    case FPCompareOp::EQ:
    case FPCompareOp::EQU:
        return ir.FPEqual(operand_1, operand_2, control, ordered);
    case FPCompareOp::LE:
    case FPCompareOp::LEU:
        return ir.FPLessThanEqual(operand_1, operand_2, control, ordered);
    case FPCompareOp::GT:
    case FPCompareOp::GTU:
        return ir.FPGreaterThan(operand_1, operand_2, control, ordered);
    case FPCompareOp::NE:
    case FPCompareOp::NEU:
        return ir.FPNotEqual(operand_1, operand_2, control, ordered);
    case FPCompareOp::GE:
    case FPCompareOp::GEU:
        return ir.FPGreaterThanEqual(operand_1, operand_2, control, ordered);
    case FPCompareOp::NUM:
        return ir.FPOrdered(operand_1, operand_2);
    case FPCompareOp::Nan:
        return ir.FPUnordered(operand_1, operand_2);
    case FPCompareOp::T:
        return ir.Imm1(true);
    default:
        throw NotImplementedException("Invalid FP compare op {}", compare_op);
    }
}
} // namespace Shader::Maxwell
