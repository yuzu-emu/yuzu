// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
void BFI(TranslatorVisitor& v, u64 insn, const IR::U32& src_a, const IR::U32& base) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> insert_reg;
        BitField<47, 1, u64> cc;
    } const bfi{insn};

    const IR::U32 zero{v.ir.Imm32(0)};
    const IR::U32 offset{v.ir.BitFieldExtract(src_a, zero, v.ir.Imm32(8), false)};
    const IR::U32 unsafe_count{v.ir.BitFieldExtract(src_a, v.ir.Imm32(8), v.ir.Imm32(8), false)};
    const IR::U32 max_size{v.ir.Imm32(32)};

    // Edge case conditions
    const IR::U1 exceed_offset{v.ir.IGreaterThanEqual(offset, max_size, false)};
    const IR::U1 exceed_count{v.ir.IGreaterThan(unsafe_count, max_size, false)};

    const IR::U32 remaining_size{v.ir.ISub(max_size, offset)};
    const IR::U32 safe_count{v.ir.Select(exceed_count, remaining_size, unsafe_count)};

    const IR::U32 insert{v.X(bfi.insert_reg)};
    IR::U32 result{v.ir.BitFieldInsert(base, insert, offset, safe_count)};

    result = IR::U32{v.ir.Select(exceed_offset, base, result)};

    v.X(bfi.dest_reg, result);
    if (bfi.cc != 0) {
        v.SetZFlag(v.ir.IEqual(result, zero));
        v.SetSFlag(v.ir.ILessThan(result, zero, true));
        v.ResetCFlag();
        v.ResetOFlag();
    }
}
} // Anonymous namespace

void TranslatorVisitor::BFI_reg(u64 insn) {
    BFI(*this, insn, GetReg20(insn), GetReg39(insn));
}

void TranslatorVisitor::BFI_rc(u64 insn) {
    BFI(*this, insn, GetReg39(insn), GetCbuf(insn));
}

void TranslatorVisitor::BFI_cr(u64 insn) {
    BFI(*this, insn, GetCbuf(insn), GetReg39(insn));
}

void TranslatorVisitor::BFI_imm(u64 insn) {
    BFI(*this, insn, GetImm20(insn), GetReg39(insn));
}

} // namespace Shader::Maxwell
