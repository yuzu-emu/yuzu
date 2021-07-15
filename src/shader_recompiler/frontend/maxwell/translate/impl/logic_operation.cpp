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

void LOP(TranslatorVisitor& v, u64 insn, IR::U32 op_b, bool x, bool cc, bool inv_a, bool inv_b,
         LogicalOp bit_op, std::optional<PredicateOp> pred_op = std::nullopt,
         IR::Pred dest_pred = IR::Pred::PT) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_reg;
    } const lop{insn};

    if (x) {
        throw NotImplementedException("X");
    }
    IR::U32 op_a{v.X(lop.src_reg)};
    if (inv_a != 0) {
        op_a = v.ir.BitwiseNot(op_a);
    }
    if (inv_b != 0) {
        op_b = v.ir.BitwiseNot(op_b);
    }

    const IR::U32 result{LogicalOperation(v.ir, op_a, op_b, bit_op)};
    if (pred_op) {
        const IR::U1 pred_result{PredicateOperation(v.ir, result, *pred_op)};
        v.ir.SetPred(dest_pred, pred_result);
    }
    if (cc) {
        if (bit_op == LogicalOp::PASS_B) {
            v.SetZFlag(v.ir.IEqual(result, v.ir.Imm32(0)));
            v.SetSFlag(v.ir.ILessThan(result, v.ir.Imm32(0), true));
        } else {
            v.SetZFlag(v.ir.GetZeroFromOp(result));
            v.SetSFlag(v.ir.GetSignFromOp(result));
        }
        v.ResetCFlag();
        v.ResetOFlag();
    }
    v.X(lop.dest_reg, result);
}

void LOP(TranslatorVisitor& v, u64 insn, const IR::U32& op_b) {
    union {
        u64 insn;
        BitField<39, 1, u64> inv_a;
        BitField<40, 1, u64> inv_b;
        BitField<41, 2, LogicalOp> bit_op;
        BitField<43, 1, u64> x;
        BitField<44, 2, PredicateOp> pred_op;
        BitField<47, 1, u64> cc;
        BitField<48, 3, IR::Pred> dest_pred;
    } const lop{insn};

    LOP(v, insn, op_b, lop.x != 0, lop.cc != 0, lop.inv_a != 0, lop.inv_b != 0, lop.bit_op,
        lop.pred_op, lop.dest_pred);
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

void TranslatorVisitor::LOP32I(u64 insn) {
    union {
        u64 raw;
        BitField<53, 2, LogicalOp> bit_op;
        BitField<57, 1, u64> x;
        BitField<52, 1, u64> cc;
        BitField<55, 1, u64> inv_a;
        BitField<56, 1, u64> inv_b;
    } const lop32i{insn};

    LOP(*this, insn, GetImm32(insn), lop32i.x != 0, lop32i.cc != 0, lop32i.inv_a != 0,
        lop32i.inv_b != 0, lop32i.bit_op);
}
} // namespace Shader::Maxwell
