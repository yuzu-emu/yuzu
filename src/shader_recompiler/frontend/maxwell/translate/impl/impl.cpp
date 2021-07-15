// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
[[nodiscard]] IR::U32 CbufLowerBits(IR::IREmitter& ir, bool unaligned, const IR::U32& binding,
                                    u32 offset) {
    if (unaligned) {
        return ir.Imm32(0);
    }
    return ir.GetCbuf(binding, IR::U32{IR::Value{offset}});
}
} // Anonymous namespace

IR::U32 TranslatorVisitor::X(IR::Reg reg) {
    return ir.GetReg(reg);
}

IR::U64 TranslatorVisitor::L(IR::Reg reg) {
    if (!IR::IsAligned(reg, 2)) {
        throw NotImplementedException("Unaligned source register {}", reg);
    }
    return IR::U64{ir.PackUint2x32(ir.CompositeConstruct(X(reg), X(reg + 1)))};
}

IR::F32 TranslatorVisitor::F(IR::Reg reg) {
    return ir.BitCast<IR::F32>(X(reg));
}

IR::F64 TranslatorVisitor::D(IR::Reg reg) {
    if (!IR::IsAligned(reg, 2)) {
        throw NotImplementedException("Unaligned source register {}", reg);
    }
    return IR::F64{ir.PackDouble2x32(ir.CompositeConstruct(X(reg), X(reg + 1)))};
}

void TranslatorVisitor::X(IR::Reg dest_reg, const IR::U32& value) {
    ir.SetReg(dest_reg, value);
}

void TranslatorVisitor::L(IR::Reg dest_reg, const IR::U64& value) {
    if (!IR::IsAligned(dest_reg, 2)) {
        throw NotImplementedException("Unaligned destination register {}", dest_reg);
    }
    const IR::Value result{ir.UnpackUint2x32(value)};
    for (int i = 0; i < 2; i++) {
        X(dest_reg + i, IR::U32{ir.CompositeExtract(result, static_cast<size_t>(i))});
    }
}

void TranslatorVisitor::F(IR::Reg dest_reg, const IR::F32& value) {
    X(dest_reg, ir.BitCast<IR::U32>(value));
}

void TranslatorVisitor::D(IR::Reg dest_reg, const IR::F64& value) {
    if (!IR::IsAligned(dest_reg, 2)) {
        throw NotImplementedException("Unaligned destination register {}", dest_reg);
    }
    const IR::Value result{ir.UnpackDouble2x32(value)};
    for (int i = 0; i < 2; i++) {
        X(dest_reg + i, IR::U32{ir.CompositeExtract(result, static_cast<size_t>(i))});
    }
}

IR::U32 TranslatorVisitor::GetReg8(u64 insn) {
    union {
        u64 raw;
        BitField<8, 8, IR::Reg> index;
    } const reg{insn};
    return X(reg.index);
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

IR::F32 TranslatorVisitor::GetFloatReg8(u64 insn) {
    return ir.BitCast<IR::F32>(GetReg8(insn));
}

IR::F32 TranslatorVisitor::GetFloatReg20(u64 insn) {
    return ir.BitCast<IR::F32>(GetReg20(insn));
}

IR::F32 TranslatorVisitor::GetFloatReg39(u64 insn) {
    return ir.BitCast<IR::F32>(GetReg39(insn));
}

IR::F64 TranslatorVisitor::GetDoubleReg20(u64 insn) {
    union {
        u64 raw;
        BitField<20, 8, IR::Reg> index;
    } const reg{insn};
    return D(reg.index);
}

IR::F64 TranslatorVisitor::GetDoubleReg39(u64 insn) {
    union {
        u64 raw;
        BitField<39, 8, IR::Reg> index;
    } const reg{insn};
    return D(reg.index);
}

static std::pair<IR::U32, IR::U32> CbufAddr(u64 insn) {
    union {
        u64 raw;
        BitField<20, 14, u64> offset;
        BitField<34, 5, u64> binding;
    } const cbuf{insn};

    if (cbuf.binding >= 18) {
        throw NotImplementedException("Out of bounds constant buffer binding {}", cbuf.binding);
    }
    if (cbuf.offset >= 0x10'000) {
        throw NotImplementedException("Out of bounds constant buffer offset {}", cbuf.offset);
    }
    const IR::Value binding{static_cast<u32>(cbuf.binding)};
    const IR::Value byte_offset{static_cast<u32>(cbuf.offset) * 4};
    return {IR::U32{binding}, IR::U32{byte_offset}};
}

IR::U32 TranslatorVisitor::GetCbuf(u64 insn) {
    const auto [binding, byte_offset]{CbufAddr(insn)};
    return ir.GetCbuf(binding, byte_offset);
}

IR::F32 TranslatorVisitor::GetFloatCbuf(u64 insn) {
    const auto [binding, byte_offset]{CbufAddr(insn)};
    return ir.GetFloatCbuf(binding, byte_offset);
}

IR::F64 TranslatorVisitor::GetDoubleCbuf(u64 insn) {
    union {
        u64 raw;
        BitField<20, 1, u64> unaligned;
    } const cbuf{insn};

    const auto [binding, offset_value]{CbufAddr(insn)};
    const bool unaligned{cbuf.unaligned != 0};
    const u32 offset{offset_value.U32()};
    const IR::Value addr{unaligned ? offset | 4u : (offset & ~7u) | 4u};

    const IR::U32 value{ir.GetCbuf(binding, IR::U32{addr})};
    const IR::U32 lower_bits{CbufLowerBits(ir, unaligned, binding, offset)};
    return ir.PackDouble2x32(ir.CompositeConstruct(lower_bits, value));
}

IR::U64 TranslatorVisitor::GetPackedCbuf(u64 insn) {
    union {
        u64 raw;
        BitField<20, 1, u64> unaligned;
    } const cbuf{insn};

    if (cbuf.unaligned != 0) {
        throw NotImplementedException("Unaligned packed constant buffer read");
    }
    const auto [binding, lower_offset]{CbufAddr(insn)};
    const IR::U32 upper_offset{ir.Imm32(lower_offset.U32() + 4)};
    const IR::U32 lower_value{ir.GetCbuf(binding, lower_offset)};
    const IR::U32 upper_value{ir.GetCbuf(binding, upper_offset)};
    return ir.PackUint2x32(ir.CompositeConstruct(lower_value, upper_value));
}

IR::U32 TranslatorVisitor::GetImm20(u64 insn) {
    union {
        u64 raw;
        BitField<20, 19, u64> value;
        BitField<56, 1, u64> is_negative;
    } const imm{insn};

    if (imm.is_negative != 0) {
        const s64 raw{static_cast<s64>(imm.value)};
        return ir.Imm32(static_cast<s32>(-(1LL << 19) + raw));
    } else {
        return ir.Imm32(static_cast<u32>(imm.value));
    }
}

IR::F32 TranslatorVisitor::GetFloatImm20(u64 insn) {
    union {
        u64 raw;
        BitField<20, 19, u64> value;
        BitField<56, 1, u64> is_negative;
    } const imm{insn};
    const u32 sign_bit{static_cast<u32>(imm.is_negative != 0 ? (1ULL << 31) : 0)};
    const u32 value{static_cast<u32>(imm.value) << 12};
    return ir.Imm32(Common::BitCast<f32>(value | sign_bit));
}

IR::F64 TranslatorVisitor::GetDoubleImm20(u64 insn) {
    union {
        u64 raw;
        BitField<20, 19, u64> value;
        BitField<56, 1, u64> is_negative;
    } const imm{insn};
    const u64 sign_bit{imm.is_negative != 0 ? (1ULL << 63) : 0};
    const u64 value{imm.value << 44};
    return ir.Imm64(Common::BitCast<f64>(value | sign_bit));
}

IR::U64 TranslatorVisitor::GetPackedImm20(u64 insn) {
    const s64 value{GetImm20(insn).U32()};
    return ir.Imm64(static_cast<u64>(static_cast<s64>(value) << 32));
}

IR::U32 TranslatorVisitor::GetImm32(u64 insn) {
    union {
        u64 raw;
        BitField<20, 32, u64> value;
    } const imm{insn};
    return ir.Imm32(static_cast<u32>(imm.value));
}

IR::F32 TranslatorVisitor::GetFloatImm32(u64 insn) {
    union {
        u64 raw;
        BitField<20, 32, u64> value;
    } const imm{insn};
    return ir.Imm32(Common::BitCast<f32>(static_cast<u32>(imm.value)));
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
