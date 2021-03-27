// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/video_helper.h"

namespace Shader::Maxwell {
void TranslatorVisitor::VMAD(u64 insn) {
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
        BitField<51, 2, u64> scale;
        BitField<53, 1, u64> src_c_neg;
        BitField<54, 1, u64> src_a_neg;
        BitField<55, 1, u64> sat;
    } const vmad{insn};

    if (vmad.cc != 0) {
        throw NotImplementedException("VMAD CC");
    }
    if (vmad.sat != 0) {
        throw NotImplementedException("VMAD SAT");
    }
    if (vmad.scale != 0) {
        throw NotImplementedException("VMAD SCALE");
    }
    if (vmad.src_a_neg != 0 && vmad.src_c_neg != 0) {
        throw NotImplementedException("VMAD PO");
    }
    if (vmad.src_a_neg != 0 || vmad.src_c_neg != 0) {
        throw NotImplementedException("VMAD NEG");
    }
    const bool is_b_imm{vmad.is_src_b_reg == 0};
    const IR::U32 src_a{GetReg8(insn)};
    const IR::U32 src_b{is_b_imm ? ir.Imm32(static_cast<u32>(vmad.src_b_imm)) : GetReg20(insn)};
    const IR::U32 src_c{GetReg39(insn)};

    const u32 a_selector{static_cast<u32>(vmad.src_a_selector)};
    // Immediate values can't have a selector
    const u32 b_selector{is_b_imm ? 0U : static_cast<u32>(vmad.src_b_selector)};
    const VideoWidth a_width{vmad.src_a_width};
    const VideoWidth b_width{GetVideoSourceWidth(vmad.src_b_width, is_b_imm)};

    const bool src_a_signed{vmad.src_a_sign != 0};
    const bool src_b_signed{vmad.src_b_sign != 0};
    const IR::U32 op_a{ExtractVideoOperandValue(ir, src_a, a_width, a_selector, src_a_signed)};
    const IR::U32 op_b{ExtractVideoOperandValue(ir, src_b, b_width, b_selector, src_b_signed)};

    X(vmad.dest_reg, ir.IAdd(ir.IMul(op_a, op_b), src_c));
}

} // namespace Shader::Maxwell
