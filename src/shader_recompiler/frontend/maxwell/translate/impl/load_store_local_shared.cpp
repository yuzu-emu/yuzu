// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class Size : u64 {
    U8,
    S8,
    U16,
    S16,
    B32,
    B64,
    B128,
};

IR::U32 Offset(TranslatorVisitor& v, u64 insn) {
    union {
        u64 raw;
        BitField<8, 8, IR::Reg> offset_reg;
        BitField<20, 24, u64> absolute_offset;
        BitField<20, 24, s64> relative_offset;
    } const encoding{insn};

    if (encoding.offset_reg == IR::Reg::RZ) {
        return v.ir.Imm32(static_cast<u32>(encoding.absolute_offset));
    } else {
        const s32 relative{static_cast<s32>(encoding.relative_offset.Value())};
        return v.ir.IAdd(v.X(encoding.offset_reg), v.ir.Imm32(relative));
    }
}

std::pair<IR::U32, IR::U32> WordOffset(TranslatorVisitor& v, u64 insn) {
    const IR::U32 offset{Offset(v, insn)};
    if (offset.IsImmediate()) {
        return {v.ir.Imm32(offset.U32() / 4), offset};
    } else {
        return {v.ir.ShiftRightArithmetic(offset, v.ir.Imm32(2)), offset};
    }
}

std::pair<int, bool> GetSize(u64 insn) {
    union {
        u64 raw;
        BitField<48, 3, Size> size;
    } const encoding{insn};

    switch (encoding.size) {
    case Size::U8:
        return {8, false};
    case Size::S8:
        return {8, true};
    case Size::U16:
        return {16, false};
    case Size::S16:
        return {16, true};
    case Size::B32:
        return {32, false};
    case Size::B64:
        return {64, false};
    case Size::B128:
        return {128, false};
    default:
        throw NotImplementedException("Invalid size {}", encoding.size.Value());
    }
}

IR::Reg Reg(u64 insn) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> reg;
    } const encoding{insn};

    return encoding.reg;
}

IR::U32 ByteOffset(IR::IREmitter& ir, const IR::U32& offset) {
    return ir.BitwiseAnd(ir.ShiftLeftLogical(offset, ir.Imm32(3)), ir.Imm32(24));
}

IR::U32 ShortOffset(IR::IREmitter& ir, const IR::U32& offset) {
    return ir.BitwiseAnd(ir.ShiftLeftLogical(offset, ir.Imm32(3)), ir.Imm32(16));
}
} // Anonymous namespace

void TranslatorVisitor::LDL(u64 insn) {
    const auto [word_offset, offset]{WordOffset(*this, insn)};
    const IR::Reg dest{Reg(insn)};
    const auto [bit_size, is_signed]{GetSize(insn)};
    switch (bit_size) {
    case 8: {
        const IR::U32 bit{ByteOffset(ir, offset)};
        X(dest, ir.BitFieldExtract(ir.LoadLocal(word_offset), bit, ir.Imm32(8), is_signed));
        break;
    }
    case 16: {
        const IR::U32 bit{ShortOffset(ir, offset)};
        X(dest, ir.BitFieldExtract(ir.LoadLocal(word_offset), bit, ir.Imm32(16), is_signed));
        break;
    }
    case 32:
    case 64:
    case 128:
        if (!IR::IsAligned(dest, static_cast<size_t>(bit_size / 32))) {
            throw NotImplementedException("Unaligned destination register {}", dest);
        }
        X(dest, ir.LoadLocal(word_offset));
        for (int i = 1; i < bit_size / 32; ++i) {
            X(dest + i, ir.LoadLocal(ir.IAdd(word_offset, ir.Imm32(i))));
        }
        break;
    }
}

void TranslatorVisitor::LDS(u64 insn) {
    const IR::U32 offset{Offset(*this, insn)};
    const IR::Reg dest{Reg(insn)};
    const auto [bit_size, is_signed]{GetSize(insn)};
    const IR::Value value{ir.LoadShared(bit_size, is_signed, offset)};
    switch (bit_size) {
    case 8:
    case 16:
    case 32:
        X(dest, IR::U32{value});
        break;
    case 64:
    case 128:
        if (!IR::IsAligned(dest, static_cast<size_t>(bit_size / 32))) {
            throw NotImplementedException("Unaligned destination register {}", dest);
        }
        for (int element = 0; element < bit_size / 32; ++element) {
            X(dest + element, IR::U32{ir.CompositeExtract(value, static_cast<size_t>(element))});
        }
        break;
    }
}

void TranslatorVisitor::STL(u64 insn) {
    const auto [word_offset, offset]{WordOffset(*this, insn)};
    const IR::Reg reg{Reg(insn)};
    const IR::U32 src{X(reg)};
    const int bit_size{GetSize(insn).first};
    switch (bit_size) {
    case 8: {
        const IR::U32 bit{ByteOffset(ir, offset)};
        const IR::U32 value{ir.BitFieldInsert(ir.LoadLocal(word_offset), src, bit, ir.Imm32(8))};
        ir.WriteLocal(word_offset, value);
        break;
    }
    case 16: {
        const IR::U32 bit{ShortOffset(ir, offset)};
        const IR::U32 value{ir.BitFieldInsert(ir.LoadLocal(word_offset), src, bit, ir.Imm32(16))};
        ir.WriteLocal(word_offset, value);
        break;
    }
    case 32:
    case 64:
    case 128:
        if (!IR::IsAligned(reg, static_cast<size_t>(bit_size / 32))) {
            throw NotImplementedException("Unaligned source register");
        }
        ir.WriteLocal(word_offset, src);
        for (int i = 1; i < bit_size / 32; ++i) {
            ir.WriteLocal(ir.IAdd(word_offset, ir.Imm32(i)), X(reg + i));
        }
        break;
    }
}

void TranslatorVisitor::STS(u64 insn) {
    const IR::U32 offset{Offset(*this, insn)};
    const IR::Reg reg{Reg(insn)};
    const int bit_size{GetSize(insn).first};
    switch (bit_size) {
    case 8:
    case 16:
    case 32:
        ir.WriteShared(bit_size, offset, X(reg));
        break;
    case 64:
        if (!IR::IsAligned(reg, 2)) {
            throw NotImplementedException("Unaligned source register {}", reg);
        }
        ir.WriteShared(64, offset, ir.CompositeConstruct(X(reg), X(reg + 1)));
        break;
    case 128: {
        if (!IR::IsAligned(reg, 2)) {
            throw NotImplementedException("Unaligned source register {}", reg);
        }
        const IR::Value vector{ir.CompositeConstruct(X(reg), X(reg + 1), X(reg + 2), X(reg + 3))};
        ir.WriteShared(128, offset, vector);
        break;
    }
    }
}

} // namespace Shader::Maxwell
