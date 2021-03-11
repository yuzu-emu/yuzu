// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/common_funcs.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class FPCompareOp : u64 {
    F,
    LT,
    EQ,
    LE,
    GT,
    NE,
    GE,
    NUM,
    Nan,
    LTU,
    EQU,
    LEU,
    GTU,
    NEU,
    GEU,
    T,
};

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

IR::U1 FloatingPointCompare(IR::IREmitter& ir, const IR::F32& operand_1, const IR::F32& operand_2,
                            FPCompareOp compare_op, IR::FpControl control) {
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
        throw NotImplementedException("Invalid compare op {}", compare_op);
    }
}

void FCMP(TranslatorVisitor& v, u64 insn, const IR::U32& src_a, const IR::F32& operand) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_reg;
        BitField<47, 1, u64> ftz;
        BitField<48, 4, FPCompareOp> compare_op;
    } const fcmp{insn};

    const IR::F32 zero{v.ir.Imm32(0.0f)};
    const IR::F32 neg_zero{v.ir.Imm32(-0.0f)};
    IR::FpControl control{.fmz_mode{fcmp.ftz != 0 ? IR::FmzMode::FTZ : IR::FmzMode::None}};
    const IR::U1 cmp_result{FloatingPointCompare(v.ir, operand, zero, fcmp.compare_op, control)};
    const IR::U32 src_reg{v.X(fcmp.src_reg)};
    const IR::U32 result{v.ir.Select(cmp_result, src_reg, src_a)};

    v.X(fcmp.dest_reg, result);
}
} // Anonymous namespace

void TranslatorVisitor::FCMP_reg(u64 insn) {
    FCMP(*this, insn, GetReg20(insn), GetFloatReg39(insn));
}

void TranslatorVisitor::FCMP_rc(u64 insn) {
    FCMP(*this, insn, GetReg39(insn), GetFloatCbuf(insn));
}

void TranslatorVisitor::FCMP_cr(u64 insn) {
    FCMP(*this, insn, GetCbuf(insn), GetFloatReg39(insn));
}

void TranslatorVisitor::FCMP_imm(u64 insn) {
    FCMP(*this, insn, GetReg39(insn), GetFloatImm20(insn));
}

} // namespace Shader::Maxwell
