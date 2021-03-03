// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/common_funcs.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class LogicalOp : u64 {
    AND,
    OR,
    XOR,
    PASS_B,
};

[[nodiscard]] IR::U32 LogicalOperation(IR::IREmitter& ir, const IR::U32& operand_1,
                                       const IR::U32& operand_2, LogicalOp op) {
    switch (op) {
    case LogicalOp::AND:
        return ir.BitwiseAnd(operand_1, operand_2);
    case LogicalOp::OR:
        return ir.BitwiseOr(operand_1, operand_2);
    case LogicalOp::XOR:
        return ir.BitwiseXor(operand_1, operand_2);
    case LogicalOp::PASS_B:
        return operand_2;
    default:
        throw NotImplementedException("Invalid Logical operation {}", op);
    }
}

void LOP(TranslatorVisitor& v, u64 insn, IR::U32 op_b) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_reg;
        BitField<39, 1, u64> neg_a;
        BitField<40, 1, u64> neg_b;
        BitField<41, 2, LogicalOp> bit_op;
        BitField<43, 1, u64> x;
        BitField<44, 2, PredicateOp> pred_op;
        BitField<48, 3, IR::Pred> pred;
    } const lop{insn};

    if (lop.x != 0) {
        throw NotImplementedException("LOP X");
    }
    IR::U32 op_a{v.X(lop.src_reg)};
    if (lop.neg_a != 0) {
        op_a = v.ir.BitwiseNot(op_a);
    }
    if (lop.neg_b != 0) {
        op_b = v.ir.BitwiseNot(op_b);
    }

    const IR::U32 result{LogicalOperation(v.ir, op_a, op_b, lop.bit_op)};
    const IR::U1 pred_result{PredicateOperation(v.ir, result, lop.pred_op)};
    v.X(lop.dest_reg, result);
    v.ir.SetPred(lop.pred, pred_result);
}
} // Anonymous namespace

void TranslatorVisitor::LOP_reg(u64 insn) {
    LOP(*this, insn, GetReg20(insn));
}

void TranslatorVisitor::LOP_cbuf(u64 insn) {
    LOP(*this, insn, GetCbuf(insn));
}

void TranslatorVisitor::LOP_imm(u64 insn) {
    LOP(*this, insn, GetImm20(insn));
}
} // namespace Shader::Maxwell
