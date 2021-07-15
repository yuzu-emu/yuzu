// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
void SHL(TranslatorVisitor& v, u64 insn, const IR::U32& unsafe_shift) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_reg_a;
        BitField<39, 1, u64> w;
        BitField<43, 1, u64> x;
        BitField<47, 1, u64> cc;
    } const shl{insn};

    if (shl.x != 0) {
        throw NotImplementedException("SHL.X");
    }
    if (shl.cc != 0) {
        throw NotImplementedException("SHL.CC");
    }
    const IR::U32 base{v.X(shl.src_reg_a)};
    IR::U32 result;
    if (shl.w != 0) {
        // When .W is set, the shift value is wrapped
        // To emulate this we just have to wrap it ourselves.
        const IR::U32 shift{v.ir.BitwiseAnd(unsafe_shift, v.ir.Imm32(31))};
        result = v.ir.ShiftLeftLogical(base, shift);
    } else {
        // When .W is not set, the shift value is clamped between 0 and 32.
        // To emulate this we have to have in mind the special shift of 32, that evaluates as 0.
        // We can safely evaluate an out of bounds shift according to the SPIR-V specification:
        //
        // https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#OpShiftLeftLogical
        // "Shift is treated as unsigned. The resulting value is undefined if Shift is greater than
        //  or equal to the bit width of the components of Base."
        //
        // And on the GLASM specification it is also safe to evaluate out of bounds:
        //
        // https://www.khronos.org/registry/OpenGL/extensions/NV/NV_gpu_program4.txt
        // "The results of a shift operation ("<<") are undefined if the value of the second operand
        //  is negative, or greater than or equal to the number of bits in the first operand."
        //
        // Emphasis on undefined results in contrast to undefined behavior.
        //
        const IR::U1 is_safe{v.ir.ILessThan(unsafe_shift, v.ir.Imm32(32), false)};
        const IR::U32 unsafe_result{v.ir.ShiftLeftLogical(base, unsafe_shift)};
        result = IR::U32{v.ir.Select(is_safe, unsafe_result, v.ir.Imm32(0))};
    }
    v.X(shl.dest_reg, result);
}
} // Anonymous namespace

void TranslatorVisitor::SHL_reg(u64 insn) {
    SHL(*this, insn, GetReg20(insn));
}

void TranslatorVisitor::SHL_cbuf(u64 insn) {
    SHL(*this, insn, GetCbuf(insn));
}

void TranslatorVisitor::SHL_imm(u64 insn) {
    SHL(*this, insn, GetImm20(insn));
}

} // namespace Shader::Maxwell
