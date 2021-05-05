// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/common_funcs.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
IR::U1 ExtendedIntegerCompare(IR::IREmitter& ir, const IR::U32& operand_1, const IR::U32& operand_2,
                              CompareOp compare_op, bool is_signed) {
    const IR::U32 zero{ir.Imm32(0)};
    const IR::U32 carry{ir.Select(ir.GetCFlag(), ir.Imm32(1), zero)};
    const IR::U1 z_flag{ir.GetZFlag()};
    const IR::U32 intermediate{ir.IAdd(ir.IAdd(operand_1, ir.BitwiseNot(operand_2)), carry)};
    const IR::U1 flip_logic{is_signed ? ir.Imm1(false)
                                      : ir.LogicalXor(ir.ILessThan(operand_1, zero, true),
                                                      ir.ILessThan(operand_2, zero, true))};
    switch (compare_op) {
    case CompareOp::False:
        return ir.Imm1(false);
    case CompareOp::LessThan:
        return IR::U1{ir.Select(flip_logic, ir.IGreaterThanEqual(intermediate, zero, true),
                                ir.ILessThan(intermediate, zero, true))};
    case CompareOp::Equal:
        return ir.LogicalAnd(ir.IEqual(intermediate, zero), z_flag);
    case CompareOp::LessThanEqual: {
        const IR::U1 base_cmp{ir.Select(flip_logic, ir.IGreaterThanEqual(intermediate, zero, true),
                                        ir.ILessThan(intermediate, zero, true))};
        return ir.LogicalOr(base_cmp, ir.LogicalAnd(ir.IEqual(intermediate, zero), z_flag));
    }
    case CompareOp::GreaterThan: {
        const IR::U1 base_cmp{ir.Select(flip_logic, ir.ILessThanEqual(intermediate, zero, true),
                                        ir.IGreaterThan(intermediate, zero, true))};
        const IR::U1 not_z{ir.LogicalNot(z_flag)};
        return ir.LogicalOr(base_cmp, ir.LogicalAnd(ir.IEqual(intermediate, zero), not_z));
    }
    case CompareOp::NotEqual:
        return ir.LogicalOr(ir.INotEqual(intermediate, zero),
                            ir.LogicalAnd(ir.IEqual(intermediate, zero), ir.LogicalNot(z_flag)));
    case CompareOp::GreaterThanEqual: {
        const IR::U1 base_cmp{ir.Select(flip_logic, ir.ILessThan(intermediate, zero, true),
                                        ir.IGreaterThanEqual(intermediate, zero, true))};
        return ir.LogicalOr(base_cmp, ir.LogicalAnd(ir.IEqual(intermediate, zero), z_flag));
    }
    case CompareOp::True:
        return ir.Imm1(true);
    default:
        throw NotImplementedException("Invalid compare op {}", compare_op);
    }
}

IR::U1 IsetCompare(IR::IREmitter& ir, const IR::U32& operand_1, const IR::U32& operand_2,
                   CompareOp compare_op, bool is_signed, bool x) {
    return x ? ExtendedIntegerCompare(ir, operand_1, operand_2, compare_op, is_signed)
             : IntegerCompare(ir, operand_1, operand_2, compare_op, is_signed);
}

void ISET(TranslatorVisitor& v, u64 insn, const IR::U32& src_b) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_reg;
        BitField<39, 3, IR::Pred> pred;
        BitField<42, 1, u64> neg_pred;
        BitField<43, 1, u64> x;
        BitField<44, 1, u64> bf;
        BitField<45, 2, BooleanOp> bop;
        BitField<47, 1, u64> cc;
        BitField<48, 1, u64> is_signed;
        BitField<49, 3, CompareOp> compare_op;
    } const iset{insn};

    const IR::U32 src_a{v.X(iset.src_reg)};
    const bool is_signed{iset.is_signed != 0};
    const IR::U32 zero{v.ir.Imm32(0)};
    const bool x{iset.x != 0};
    const IR::U1 cmp_result{IsetCompare(v.ir, src_a, src_b, iset.compare_op, is_signed, x)};

    IR::U1 pred{v.ir.GetPred(iset.pred)};
    if (iset.neg_pred != 0) {
        pred = v.ir.LogicalNot(pred);
    }
    const IR::U1 bop_result{PredicateCombine(v.ir, cmp_result, pred, iset.bop)};

    const IR::U32 one_mask{v.ir.Imm32(-1)};
    const IR::U32 fp_one{v.ir.Imm32(0x3f800000)};
    const IR::U32 pass_result{iset.bf == 0 ? one_mask : fp_one};
    const IR::U32 result{v.ir.Select(bop_result, pass_result, zero)};

    v.X(iset.dest_reg, result);
    if (iset.cc != 0) {
        if (x) {
            throw NotImplementedException("ISET.CC + X");
        }
        const IR::U1 is_zero{v.ir.IEqual(result, zero)};
        v.SetZFlag(is_zero);
        if (iset.bf != 0) {
            v.ResetSFlag();
        } else {
            v.SetSFlag(v.ir.LogicalNot(is_zero));
        }
        v.ResetCFlag();
        v.ResetOFlag();
    }
}
} // Anonymous namespace

void TranslatorVisitor::ISET_reg(u64 insn) {
    ISET(*this, insn, GetReg20(insn));
}

void TranslatorVisitor::ISET_cbuf(u64 insn) {
    ISET(*this, insn, GetCbuf(insn));
}

void TranslatorVisitor::ISET_imm(u64 insn) {
    ISET(*this, insn, GetImm20(insn));
}

} // namespace Shader::Maxwell
