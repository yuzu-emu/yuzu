// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {

IR::U32 TranslatorVisitor::X(IR::Reg reg) {
    return ir.GetReg(reg);
}

void TranslatorVisitor::X(IR::Reg dest_reg, const IR::U32& value) {
    ir.SetReg(dest_reg, value);
}

IR::U32 TranslatorVisitor::GetReg20(u64 insn) {
    union {
        u64 raw;
        BitField<20, 8, IR::Reg> index;
    } const reg{insn};
    return X(reg.index);
}

IR::U32 TranslatorVisitor::GetReg39(u64 insn) {
    union {
        u64 raw;
        BitField<39, 8, IR::Reg> index;
    } const reg{insn};
    return X(reg.index);
}

IR::U32 TranslatorVisitor::GetCbuf(u64 insn) {
    union {
        u64 raw;
        BitField<20, 14, s64> offset;
        BitField<34, 5, u64> binding;
    } const cbuf{insn};
    if (cbuf.binding >= 18) {
        throw NotImplementedException("Out of bounds constant buffer binding {}", cbuf.binding);
    }
    if (cbuf.offset >= 0x10'000 || cbuf.offset < 0) {
        throw NotImplementedException("Out of bounds constant buffer offset {}", cbuf.offset);
    }
    const IR::U32 binding{ir.Imm32(static_cast<u32>(cbuf.binding))};
    const IR::U32 byte_offset{ir.Imm32(static_cast<u32>(cbuf.offset) * 4)};
    return ir.GetCbuf(binding, byte_offset);
}

IR::U32 TranslatorVisitor::GetImm20(u64 insn) {
    union {
        u64 raw;
        BitField<20, 19, u64> value;
        BitField<56, 1, u64> is_negative;
    } const imm{insn};
    const s32 positive_value{static_cast<s32>(imm.value)};
    const s32 value{imm.is_negative != 0 ? -positive_value : positive_value};
    return ir.Imm32(value);
}

IR::U32 TranslatorVisitor::GetImm32(u64 insn) {
    union {
        u64 raw;
        BitField<20, 32, u64> value;
    } const imm{insn};
    return ir.Imm32(static_cast<u32>(imm.value));
}

void TranslatorVisitor::SetZFlag(const IR::U1& value) {
    ir.SetZFlag(value);
}

void TranslatorVisitor::SetSFlag(const IR::U1& value) {
    ir.SetSFlag(value);
}

void TranslatorVisitor::SetCFlag(const IR::U1& value) {
    ir.SetCFlag(value);
}

void TranslatorVisitor::SetOFlag(const IR::U1& value) {
    ir.SetOFlag(value);
}

void TranslatorVisitor::ResetZero() {
    SetZFlag(ir.Imm1(false));
}

void TranslatorVisitor::ResetSFlag() {
    SetSFlag(ir.Imm1(false));
}

void TranslatorVisitor::ResetCFlag() {
    SetCFlag(ir.Imm1(false));
}

void TranslatorVisitor::ResetOFlag() {
    SetOFlag(ir.Imm1(false));
}

} // namespace Shader::Maxwell
