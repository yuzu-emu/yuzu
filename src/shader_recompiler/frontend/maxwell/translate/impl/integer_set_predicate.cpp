// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class CompareOp : u64 {
    F,  // Always false
    LT, // Less than
    EQ, // Equal
    LE, // Less than or equal
    GT, // Greater than
    NE, // Not equal
    GE, // Greater than or equal
    T,  // Always true
};

enum class Bop : u64 {
    AND,
    OR,
    XOR,
};

IR::U1 Compare(IR::IREmitter& ir, CompareOp op, const IR::U32& lhs, const IR::U32& rhs,
               bool is_signed) {
    switch (op) {
    case CompareOp::F:
        return ir.Imm1(false);
    case CompareOp::LT:
        return ir.ILessThan(lhs, rhs, is_signed);
    case CompareOp::EQ:
        return ir.IEqual(lhs, rhs);
    case CompareOp::LE:
        return ir.ILessThanEqual(lhs, rhs, is_signed);
    case CompareOp::GT:
        return ir.IGreaterThan(lhs, rhs, is_signed);
    case CompareOp::NE:
        return ir.INotEqual(lhs, rhs);
    case CompareOp::GE:
        return ir.IGreaterThanEqual(lhs, rhs, is_signed);
    case CompareOp::T:
        return ir.Imm1(true);
    }
    throw NotImplementedException("Invalid ISETP compare op {}", op);
}

IR::U1 Combine(IR::IREmitter& ir, Bop bop, const IR::U1& comparison, const IR::U1& bop_pred) {
    switch (bop) {
    case Bop::AND:
        return ir.LogicalAnd(comparison, bop_pred);
    case Bop::OR:
        return ir.LogicalOr(comparison, bop_pred);
    case Bop::XOR:
        return ir.LogicalXor(comparison, bop_pred);
    }
    throw NotImplementedException("Invalid ISETP bop {}", bop);
}

void ISETP(TranslatorVisitor& v, u64 insn, const IR::U32& op_b) {
    union {
        u64 raw;
        BitField<0, 3, IR::Pred> dest_pred_b;
        BitField<3, 3, IR::Pred> dest_pred_a;
        BitField<8, 8, IR::Reg> src_reg_a;
        BitField<39, 3, IR::Pred> bop_pred;
        BitField<42, 1, u64> neg_bop_pred;
        BitField<45, 2, Bop> bop;
        BitField<48, 1, u64> is_signed;
        BitField<49, 3, CompareOp> compare_op;
    } const isetp{insn};

    const Bop bop{isetp.bop};
    const IR::U32 op_a{v.X(isetp.src_reg_a)};
    const IR::U1 comparison{Compare(v.ir, isetp.compare_op, op_a, op_b, isetp.is_signed != 0)};
    const IR::U1 bop_pred{v.ir.GetPred(isetp.bop_pred, isetp.neg_bop_pred != 0)};
    const IR::U1 result_a{Combine(v.ir, bop, comparison, bop_pred)};
    const IR::U1 result_b{Combine(v.ir, bop, v.ir.LogicalNot(comparison), bop_pred)};
    v.ir.SetPred(isetp.dest_pred_a, result_a);
    v.ir.SetPred(isetp.dest_pred_b, result_b);
}
} // Anonymous namespace

void TranslatorVisitor::ISETP_reg(u64 insn) {
    ISETP(*this, insn, GetReg20(insn));
}

void TranslatorVisitor::ISETP_cbuf(u64 insn) {
    ISETP(*this, insn, GetCbuf(insn));
}

void TranslatorVisitor::ISETP_imm(u64) {
    throw NotImplementedException("ISETP_imm");
}

} // namespace Shader::Maxwell
