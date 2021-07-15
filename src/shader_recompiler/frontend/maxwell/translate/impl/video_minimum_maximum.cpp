// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/video_helper.h"

namespace Shader::Maxwell {
namespace {
enum class VideoMinMaxOps : u64 {
    MRG_16H,
    MRG_16L,
    MRG_8B0,
    MRG_8B2,
    ACC,
    MIN,
    MAX,
};

[[nodiscard]] IR::U32 ApplyVideoMinMaxOp(IR::IREmitter& ir, const IR::U32& lhs, const IR::U32& rhs,
                                         VideoMinMaxOps op, bool is_signed) {
    switch (op) {
    case VideoMinMaxOps::MIN:
        return ir.IMin(lhs, rhs, is_signed);
    case VideoMinMaxOps::MAX:
        return ir.IMax(lhs, rhs, is_signed);
    default:
        throw NotImplementedException("VMNMX op {}", op);
    }
}
} // Anonymous namespace

void TranslatorVisitor::VMNMX(u64 insn) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<20, 16, u64> src_b_imm;
        BitField<28, 2, u64> src_b_selector;
        BitField<29, 2, VideoWidth> src_b_width;
        BitField<36, 2, u64> src_a_selector;
        BitField<37, 2, VideoWidth> src_a_width;
        BitField<47, 1, u64> cc;
        BitField<48, 1, u64> src_a_sign;
        BitField<49, 1, u64> src_b_sign;
        BitField<50, 1, u64> is_src_b_reg;
        BitField<51, 3, VideoMinMaxOps> op;
        BitField<54, 1, u64> dest_sign;
        BitField<55, 1, u64> sat;
        BitField<56, 1, u64> mx;
    } const vmnmx{insn};

    if (vmnmx.cc != 0) {
        throw NotImplementedException("VMNMX CC");
    }
    if (vmnmx.sat != 0) {
        throw NotImplementedException("VMNMX SAT");
    }
    // Selectors were shown to default to 2 in unit tests
    if (vmnmx.src_a_selector != 2) {
        throw NotImplementedException("VMNMX Selector {}", vmnmx.src_a_selector.Value());
    }
    if (vmnmx.src_b_selector != 2) {
        throw NotImplementedException("VMNMX Selector {}", vmnmx.src_b_selector.Value());
    }
    if (vmnmx.src_a_width != VideoWidth::Word) {
        throw NotImplementedException("VMNMX Source Width {}", vmnmx.src_a_width.Value());
    }

    const bool is_b_imm{vmnmx.is_src_b_reg == 0};
    const IR::U32 src_a{GetReg8(insn)};
    const IR::U32 src_b{is_b_imm ? ir.Imm32(static_cast<u32>(vmnmx.src_b_imm)) : GetReg20(insn)};
    const IR::U32 src_c{GetReg39(insn)};

    const VideoWidth a_width{vmnmx.src_a_width};
    const VideoWidth b_width{GetVideoSourceWidth(vmnmx.src_b_width, is_b_imm)};

    const bool src_a_signed{vmnmx.src_a_sign != 0};
    const bool src_b_signed{vmnmx.src_b_sign != 0};
    const IR::U32 op_a{ExtractVideoOperandValue(ir, src_a, a_width, 0, src_a_signed)};
    const IR::U32 op_b{ExtractVideoOperandValue(ir, src_b, b_width, 0, src_b_signed)};

    // First operation's sign is only dependent on operand b's sign
    const bool op_1_signed{src_b_signed};

    const IR::U32 lhs{vmnmx.mx != 0 ? ir.IMax(op_a, op_b, op_1_signed)
                                    : ir.IMin(op_a, op_b, op_1_signed)};
    X(vmnmx.dest_reg, ApplyVideoMinMaxOp(ir, lhs, src_c, vmnmx.op, vmnmx.dest_sign != 0));
}

} // namespace Shader::Maxwell
