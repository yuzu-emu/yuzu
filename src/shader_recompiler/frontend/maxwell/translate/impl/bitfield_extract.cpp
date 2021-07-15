// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
void BFE(TranslatorVisitor& v, u64 insn, const IR::U32& src) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> offset_reg;
        BitField<40, 1, u64> brev;
        BitField<47, 1, u64> cc;
        BitField<48, 1, u64> is_signed;
    } const bfe{insn};

    const IR::U32 offset{v.ir.BitFieldExtract(src, v.ir.Imm32(0), v.ir.Imm32(8), false)};
    const IR::U32 count{v.ir.BitFieldExtract(src, v.ir.Imm32(8), v.ir.Imm32(8), false)};

    // Common constants
    const IR::U32 zero{v.ir.Imm32(0)};
    const IR::U32 one{v.ir.Imm32(1)};
    const IR::U32 max_size{v.ir.Imm32(32)};
    // Edge case conditions
    const IR::U1 zero_count{v.ir.IEqual(count, zero)};
    const IR::U1 exceed_count{v.ir.IGreaterThanEqual(v.ir.IAdd(offset, count), max_size, false)};
    const IR::U1 replicate{v.ir.IGreaterThanEqual(offset, max_size, false)};

    IR::U32 base{v.X(bfe.offset_reg)};
    if (bfe.brev != 0) {
        base = v.ir.BitReverse(base);
    }
    IR::U32 result{v.ir.BitFieldExtract(base, offset, count, bfe.is_signed != 0)};
    if (bfe.is_signed != 0) {
        const IR::U1 is_negative{v.ir.ILessThan(base, zero, true)};
        const IR::U32 replicated_bit{v.ir.Select(is_negative, v.ir.Imm32(-1), zero)};
        const IR::U32 exceed_bit{v.ir.BitFieldExtract(base, v.ir.Imm32(31), one, false)};
        // Replicate condition
        result = IR::U32{v.ir.Select(replicate, replicated_bit, result)};
        // Exceeding condition
        const IR::U32 exceed_result{v.ir.BitFieldInsert(result, exceed_bit, v.ir.Imm32(31), one)};
        result = IR::U32{v.ir.Select(exceed_count, exceed_result, result)};
    }
    // Zero count condition
    result = IR::U32{v.ir.Select(zero_count, zero, result)};

    v.X(bfe.dest_reg, result);

    if (bfe.cc != 0) {
        v.SetZFlag(v.ir.IEqual(result, zero));
        v.SetSFlag(v.ir.ILessThan(result, zero, true));
        v.ResetCFlag();
        v.ResetOFlag();
    }
}
} // Anonymous namespace

void TranslatorVisitor::BFE_reg(u64 insn) {
    BFE(*this, insn, GetReg20(insn));
}

void TranslatorVisitor::BFE_cbuf(u64 insn) {
    BFE(*this, insn, GetCbuf(insn));
}

void TranslatorVisitor::BFE_imm(u64 insn) {
    BFE(*this, insn, GetImm20(insn));
}

} // namespace Shader::Maxwell
