// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class MaxShift : u64 {
    U32,
    Undefined,
    U64,
    S64,
};

IR::U64 PackedShift(IR::IREmitter& ir, const IR::U64& packed_int, const IR::U32& safe_shift,
                    bool right_shift, bool is_signed) {
    if (!right_shift) {
        return ir.ShiftLeftLogical(packed_int, safe_shift);
    }
    if (is_signed) {
        return ir.ShiftRightArithmetic(packed_int, safe_shift);
    }
    return ir.ShiftRightLogical(packed_int, safe_shift);
}

void SHF(TranslatorVisitor& v, u64 insn, const IR::U32& shift, const IR::U32& high_bits,
         bool right_shift) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<0, 8, IR::Reg> lo_bits_reg;
        BitField<37, 2, MaxShift> max_shift;
        BitField<47, 1, u64> cc;
        BitField<48, 2, u64> x_mode;
        BitField<50, 1, u64> wrap;
    } const shf{insn};

    if (shf.cc != 0) {
        throw NotImplementedException("SHF CC");
    }
    if (shf.x_mode != 0) {
        throw NotImplementedException("SHF X Mode");
    }
    if (shf.max_shift == MaxShift::Undefined) {
        throw NotImplementedException("SHF Use of undefined MaxShift value");
    }
    const IR::U32 low_bits{v.X(shf.lo_bits_reg)};
    const IR::U64 packed_int{v.ir.PackUint2x32(v.ir.CompositeConstruct(low_bits, high_bits))};
    const IR::U32 max_shift{shf.max_shift == MaxShift::U32 ? v.ir.Imm32(32) : v.ir.Imm32(63)};
    const IR::U32 safe_shift{shf.wrap != 0
                                 ? v.ir.BitwiseAnd(shift, v.ir.ISub(max_shift, v.ir.Imm32(1)))
                                 : v.ir.UMin(shift, max_shift)};

    const bool is_signed{shf.max_shift == MaxShift::S64};
    const IR::U64 shifted_value{PackedShift(v.ir, packed_int, safe_shift, right_shift, is_signed)};
    const IR::Value unpacked_value{v.ir.UnpackUint2x32(shifted_value)};

    const IR::U32 result{v.ir.CompositeExtract(unpacked_value, right_shift ? 0 : 1)};
    v.X(shf.dest_reg, result);
}
} // Anonymous namespace

void TranslatorVisitor::SHF_l_reg(u64 insn) {
    SHF(*this, insn, GetReg20(insn), GetReg39(insn), false);
}

void TranslatorVisitor::SHF_l_imm(u64 insn) {
    SHF(*this, insn, GetImm20(insn), GetReg39(insn), false);
}

void TranslatorVisitor::SHF_r_reg(u64 insn) {
    SHF(*this, insn, GetReg20(insn), GetReg39(insn), true);
}

void TranslatorVisitor::SHF_r_imm(u64 insn) {
    SHF(*this, insn, GetImm20(insn), GetReg39(insn), true);
}

} // namespace Shader::Maxwell
