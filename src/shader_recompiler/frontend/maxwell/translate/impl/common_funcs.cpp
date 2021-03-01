// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/frontend/maxwell/translate/impl/common_funcs.h"

namespace Shader::Maxwell {
[[nodiscard]] IR::U1 IntegerCompare(TranslatorVisitor& v, const IR::U32& operand_1,
                                    const IR::U32& operand_2, ComparisonOp compare_op,
                                    bool is_signed) {
    switch (compare_op) {
    case ComparisonOp::False:
        return v.ir.Imm1(false);
    case ComparisonOp::LessThan:
        return v.ir.ILessThan(operand_1, operand_2, is_signed);
    case ComparisonOp::Equal:
        return v.ir.IEqual(operand_1, operand_2);
    case ComparisonOp::LessThanEqual:
        return v.ir.ILessThanEqual(operand_1, operand_2, is_signed);
    case ComparisonOp::GreaterThan:
        return v.ir.IGreaterThan(operand_1, operand_2, is_signed);
    case ComparisonOp::NotEqual:
        return v.ir.INotEqual(operand_1, operand_2);
    case ComparisonOp::GreaterThanEqual:
        return v.ir.IGreaterThanEqual(operand_1, operand_2, is_signed);
    case ComparisonOp::True:
        return v.ir.Imm1(true);
    default:
        throw NotImplementedException("CMP");
    }
}

[[nodiscard]] IR::U1 PredicateCombine(TranslatorVisitor& v, const IR::U1& predicate_1,
                                      const IR::U1& predicate_2, BooleanOp bop) {
    switch (bop) {
    case BooleanOp::And:
        return v.ir.LogicalAnd(predicate_1, predicate_2);
    case BooleanOp::Or:
        return v.ir.LogicalOr(predicate_1, predicate_2);
    case BooleanOp::Xor:
        return v.ir.LogicalXor(predicate_1, predicate_2);
    default:
        throw NotImplementedException("BOP");
    }
}
} // namespace Shader::Maxwell
