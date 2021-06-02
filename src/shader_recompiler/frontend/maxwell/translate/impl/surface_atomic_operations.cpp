// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <bit>

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/ir/modifiers.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class Type : u64 {
    _1D,
    BUFFER_1D,
    ARRAY_1D,
    _2D,
    ARRAY_2D,
    _3D,
};

enum class Size : u64 {
    U32,
    S32,
    U64,
    S64,
    F32FTZRN,
    F16x2FTZRN,
    SD32,
    SD64,
};

enum class AtomicOp : u64 {
    ADD,
    MIN,
    MAX,
    INC,
    DEC,
    AND,
    OR,
    XOR,
    EXCH,
};

enum class Clamp : u64 {
    IGN,
    Default,
    TRAP,
};

TextureType GetType(Type type) {
    switch (type) {
    case Type::_1D:
        return TextureType::Color1D;
    case Type::BUFFER_1D:
        return TextureType::Buffer;
    case Type::ARRAY_1D:
        return TextureType::ColorArray1D;
    case Type::_2D:
        return TextureType::Color2D;
    case Type::ARRAY_2D:
        return TextureType::ColorArray2D;
    case Type::_3D:
        return TextureType::Color3D;
    }
    throw NotImplementedException("Invalid type {}", type);
}

IR::Value MakeCoords(TranslatorVisitor& v, IR::Reg reg, Type type) {
    const auto array{[&](int index) {
        return v.ir.BitFieldExtract(v.X(reg + index), v.ir.Imm32(0), v.ir.Imm32(16));
    }};
    switch (type) {
    case Type::_1D:
    case Type::BUFFER_1D:
        return v.X(reg);
    case Type::_2D:
        return v.ir.CompositeConstruct(v.X(reg), v.X(reg + 1));
    case Type::_3D:
        return v.ir.CompositeConstruct(v.X(reg), v.X(reg + 1), v.X(reg + 2));
    default:
        break;
    }
    throw NotImplementedException("Invalid type {}", type);
}

IR::Value ApplyAtomicOp(IR::IREmitter& ir, const IR::U32& handle, const IR::Value& coords,
                        const IR::Value& op_b, IR::TextureInstInfo info, AtomicOp op,
                        bool is_signed) {
    switch (op) {
    case AtomicOp::ADD:
        return ir.ImageAtomicIAdd(handle, coords, op_b, info);
    case AtomicOp::MIN:
        return ir.ImageAtomicIMin(handle, coords, op_b, is_signed, info);
    case AtomicOp::MAX:
        return ir.ImageAtomicIMax(handle, coords, op_b, is_signed, info);
    case AtomicOp::INC:
        return ir.ImageAtomicInc(handle, coords, op_b, info);
    case AtomicOp::DEC:
        return ir.ImageAtomicDec(handle, coords, op_b, info);
    case AtomicOp::AND:
        return ir.ImageAtomicAnd(handle, coords, op_b, info);
    case AtomicOp::OR:
        return ir.ImageAtomicOr(handle, coords, op_b, info);
    case AtomicOp::XOR:
        return ir.ImageAtomicXor(handle, coords, op_b, info);
    case AtomicOp::EXCH:
        return ir.ImageAtomicExchange(handle, coords, op_b, info);
    default:
        throw NotImplementedException("Atomic Operation {}", op);
    }
}

ImageFormat Format(Size size) {
    switch (size) {
    case Size::U32:
    case Size::S32:
    case Size::SD32:
        return ImageFormat::R32_UINT;
    default:
        break;
    }
    throw NotImplementedException("Invalid size {}", size);
}

bool IsSizeInt32(Size size) {
    switch (size) {
    case Size::U32:
    case Size::S32:
    case Size::SD32:
        return true;
    default:
        return false;
    }
}

void ImageAtomOp(TranslatorVisitor& v, IR::Reg dest_reg, IR::Reg operand_reg, IR::Reg coord_reg,
                 IR::Reg bindless_reg, AtomicOp op, Clamp clamp, Size size, Type type,
                 u64 bound_offset, bool is_bindless, bool write_result) {
    if (clamp != Clamp::IGN) {
        throw NotImplementedException("Clamp {}", clamp);
    }
    if (!IsSizeInt32(size)) {
        throw NotImplementedException("Size {}", size);
    }
    const bool is_signed{size == Size::S32};
    const ImageFormat format{Format(size)};
    const TextureType tex_type{GetType(type)};
    const IR::Value coords{MakeCoords(v, coord_reg, type)};

    const IR::U32 handle{is_bindless != 0 ? v.X(bindless_reg)
                                          : v.ir.Imm32(static_cast<u32>(bound_offset * 4))};
    IR::TextureInstInfo info{};
    info.type.Assign(tex_type);
    info.image_format.Assign(format);

    // TODO: float/64-bit operand
    const IR::Value op_b{v.X(operand_reg)};
    const IR::Value color{ApplyAtomicOp(v.ir, handle, coords, op_b, info, op, is_signed)};

    if (write_result) {
        v.X(dest_reg, IR::U32{color});
    }
}
} // Anonymous namespace

void TranslatorVisitor::SUATOM(u64 insn) {
    union {
        u64 raw;
        BitField<54, 1, u64> is_bindless;
        BitField<29, 4, AtomicOp> op;
        BitField<33, 3, Type> type;
        BitField<51, 3, Size> size;
        BitField<49, 2, Clamp> clamp;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> coord_reg;
        BitField<20, 8, IR::Reg> operand_reg;
        BitField<36, 13, u64> bound_offset;    // !is_bindless
        BitField<39, 8, IR::Reg> bindless_reg; // is_bindless
    } const suatom{insn};

    ImageAtomOp(*this, suatom.dest_reg, suatom.operand_reg, suatom.coord_reg, suatom.bindless_reg,
                suatom.op, suatom.clamp, suatom.size, suatom.type, suatom.bound_offset,
                suatom.is_bindless != 0, true);
}

void TranslatorVisitor::SURED(u64 insn) {
    // TODO: confirm offsets
    union {
        u64 raw;
        BitField<51, 1, u64> is_bound;
        BitField<21, 3, AtomicOp> op;
        BitField<33, 3, Type> type;
        BitField<20, 3, Size> size;
        BitField<49, 2, Clamp> clamp;
        BitField<0, 8, IR::Reg> operand_reg;
        BitField<8, 8, IR::Reg> coord_reg;
        BitField<36, 13, u64> bound_offset;    // is_bound
        BitField<39, 8, IR::Reg> bindless_reg; // !is_bound
    } const sured{insn};
    ImageAtomOp(*this, IR::Reg::RZ, sured.operand_reg, sured.coord_reg, sured.bindless_reg,
                sured.op, sured.clamp, sured.size, sured.type, sured.bound_offset,
                sured.is_bound == 0, false);
}

} // namespace Shader::Maxwell
