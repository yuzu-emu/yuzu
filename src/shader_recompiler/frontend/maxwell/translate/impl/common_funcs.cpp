// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/frontend/maxwell/translate/impl/common_funcs.h"

namespace Shader::Maxwell {
[[nodiscard]] IR::U1 IntegerCompare(IR::IREmitter& ir, const IR::U32& operand_1,
                                    const IR::U32& operand_2, CompareOp compare_op,
                                    bool is_signed) {
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

[[nodiscard]] IR::U1 PredicateCombine(IR::IREmitter& ir, const IR::U1& predicate_1,
                                      const IR::U1& predicate_2, BooleanOp bop) {
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
} // namespace Shader::Maxwell
