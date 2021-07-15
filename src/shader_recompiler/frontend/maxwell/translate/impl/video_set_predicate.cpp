// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/common_funcs.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/video_helper.h"

namespace Shader::Maxwell {
namespace {
enum class VsetpCompareOp : u64 {
    False = 0,
    LessThan,
    Equal,
    LessThanEqual,
    GreaterThan = 16,
    NotEqual,
    GreaterThanEqual,
    True,
};

CompareOp VsetpToShaderCompareOp(VsetpCompareOp op) {
    switch (op) {
    case VsetpCompareOp::False:
        return CompareOp::False;
    case VsetpCompareOp::LessThan:
        return CompareOp::LessThan;
    case VsetpCompareOp::Equal:
        return CompareOp::Equal;
    case VsetpCompareOp::LessThanEqual:
        return CompareOp::LessThanEqual;
    case VsetpCompareOp::GreaterThan:
        return CompareOp::GreaterThan;
    case VsetpCompareOp::NotEqual:
        return CompareOp::NotEqual;
    case VsetpCompareOp::GreaterThanEqual:
        return CompareOp::GreaterThanEqual;
    case VsetpCompareOp::True:
        return CompareOp::True;
    default:
        throw NotImplementedException("Invalid compare op {}", op);
    }
}
} // Anonymous namespace

void TranslatorVisitor::VSETP(u64 insn) {
    union {
        u64 raw;
        BitField<0, 3, IR::Pred> dest_pred_b;
        BitField<3, 3, IR::Pred> dest_pred_a;
        BitField<20, 16, u64> src_b_imm;
        BitField<28, 2, u64> src_b_selector;
        BitField<29, 2, VideoWidth> src_b_width;
        BitField<36, 2, u64> src_a_selector;
        BitField<37, 2, VideoWidth> src_a_width;
        BitField<39, 3, IR::Pred> bop_pred;
        BitField<42, 1, u64> neg_bop_pred;
        BitField<43, 5, VsetpCompareOp> compare_op;
        BitField<45, 2, BooleanOp> bop;
        BitField<48, 1, u64> src_a_sign;
        BitField<49, 1, u64> src_b_sign;
        BitField<50, 1, u64> is_src_b_reg;
    } const vsetp{insn};

    const bool is_b_imm{vsetp.is_src_b_reg == 0};
    const IR::U32 src_a{GetReg8(insn)};
    const IR::U32 src_b{is_b_imm ? ir.Imm32(static_cast<u32>(vsetp.src_b_imm)) : GetReg20(insn)};

    const u32 a_selector{static_cast<u32>(vsetp.src_a_selector)};
    const u32 b_selector{static_cast<u32>(vsetp.src_b_selector)};
    const VideoWidth a_width{vsetp.src_a_width};
    const VideoWidth b_width{GetVideoSourceWidth(vsetp.src_b_width, is_b_imm)};

    const bool src_a_signed{vsetp.src_a_sign != 0};
    const bool src_b_signed{vsetp.src_b_sign != 0};
    const IR::U32 op_a{ExtractVideoOperandValue(ir, src_a, a_width, a_selector, src_a_signed)};
    const IR::U32 op_b{ExtractVideoOperandValue(ir, src_b, b_width, b_selector, src_b_signed)};

    // Compare operation's sign is only dependent on operand b's sign
    const bool compare_signed{src_b_signed};
    const CompareOp compare_op{VsetpToShaderCompareOp(vsetp.compare_op)};
    const IR::U1 comparison{IntegerCompare(ir, op_a, op_b, compare_op, compare_signed)};
    const IR::U1 bop_pred{ir.GetPred(vsetp.bop_pred, vsetp.neg_bop_pred != 0)};
    const IR::U1 result_a{PredicateCombine(ir, comparison, bop_pred, vsetp.bop)};
    const IR::U1 result_b{PredicateCombine(ir, ir.LogicalNot(comparison), bop_pred, vsetp.bop)};
    ir.SetPred(vsetp.dest_pred_a, result_a);
    ir.SetPred(vsetp.dest_pred_b, result_b);
}

} // namespace Shader::Maxwell
