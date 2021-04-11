// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/common_funcs.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
void FCMP(TranslatorVisitor& v, u64 insn, const IR::U32& src_a, const IR::F32& operand) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_reg;
        BitField<47, 1, u64> ftz;
        BitField<48, 4, FPCompareOp> compare_op;
    } const fcmp{insn};

    const IR::F32 zero{v.ir.Imm32(0.0f)};
    const IR::FpControl control{.fmz_mode = (fcmp.ftz != 0 ? IR::FmzMode::FTZ : IR::FmzMode::None)};
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
    union {
        u64 raw;
        BitField<20, 19, u64> value;
        BitField<56, 1, u64> is_negative;
    } const fcmp{insn};
    const u32 sign_bit{fcmp.is_negative != 0 ? (1U << 31) : 0};
    const u32 value{static_cast<u32>(fcmp.value) << 12};

    FCMP(*this, insn, ir.Imm32(value | sign_bit), GetFloatReg39(insn));
}

} // namespace Shader::Maxwell
