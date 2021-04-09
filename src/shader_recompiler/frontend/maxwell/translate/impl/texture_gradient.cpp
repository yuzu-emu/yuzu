// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <optional>

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/ir/modifiers.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {

enum class TextureType : u64 {
    _1D,
    ARRAY_1D,
    _2D,
    ARRAY_2D,
    _3D,
    ARRAY_3D,
    CUBE,
    ARRAY_CUBE,
};

Shader::TextureType GetType(TextureType type) {
    switch (type) {
    case TextureType::_1D:
        return Shader::TextureType::Color1D;
    case TextureType::ARRAY_1D:
        return Shader::TextureType::ColorArray1D;
    case TextureType::_2D:
        return Shader::TextureType::Color2D;
    case TextureType::ARRAY_2D:
        return Shader::TextureType::ColorArray2D;
    case TextureType::_3D:
        return Shader::TextureType::Color3D;
    case TextureType::ARRAY_3D:
        throw NotImplementedException("3D array texture type");
    case TextureType::CUBE:
        return Shader::TextureType::ColorCube;
    case TextureType::ARRAY_CUBE:
        return Shader::TextureType::ColorArrayCube;
    }
    throw NotImplementedException("Invalid texture type {}", type);
}

IR::Value MakeOffset(TranslatorVisitor& v, IR::Reg reg, bool has_lod_clamp) {
    const IR::U32 value{v.X(reg)};
    const u32 base{has_lod_clamp ? 12U : 16U};
    return v.ir.CompositeConstruct(
        v.ir.BitFieldExtract(value, v.ir.Imm32(base), v.ir.Imm32(4), true),
        v.ir.BitFieldExtract(value, v.ir.Imm32(base + 4), v.ir.Imm32(4), true));
}

void Impl(TranslatorVisitor& v, u64 insn, bool is_bindless) {
    union {
        u64 raw;
        BitField<49, 1, u64> nodep;
        BitField<35, 1, u64> aoffi;
        BitField<50, 1, u64> lc;
        BitField<51, 3, IR::Pred> sparse_pred;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> coord_reg;
        BitField<20, 8, IR::Reg> derivate_reg;
        BitField<28, 3, TextureType> type;
        BitField<31, 4, u64> mask;
        BitField<36, 13, u64> cbuf_offset;
    } const txd{insn};

    const bool has_lod_clamp = txd.lc != 0;
    if (has_lod_clamp) {
        throw NotImplementedException("TXD.LC - CLAMP is not implemented");
    }

    IR::Value coords;
    u32 num_derivates{};
    IR::Reg base_reg{txd.coord_reg};
    IR::Reg last_reg;
    IR::Value handle;
    if (is_bindless) {
        handle = v.X(base_reg++);
    } else {
        handle = v.ir.Imm32(static_cast<u32>(txd.cbuf_offset.Value() * 4));
    }

    const auto read_array{[&]() -> IR::F32 {
        const IR::U32 base{v.ir.Imm32(0)};
        const IR::U32 count{v.ir.Imm32(has_lod_clamp ? 12 : 16)};
        const IR::U32 array_index{v.ir.BitFieldExtract(v.X(last_reg), base, count)};
        return v.ir.ConvertUToF(32, 16, array_index);
    }};
    switch (txd.type) {
    case TextureType::_1D: {
        coords = v.F(base_reg);
        num_derivates = 1;
        last_reg = base_reg + 1;
        break;
    }
    case TextureType::ARRAY_1D: {
        last_reg = base_reg + 1;
        coords = v.ir.CompositeConstruct(v.F(base_reg), read_array());
        num_derivates = 1;
        break;
    }
    case TextureType::_2D: {
        last_reg = base_reg + 2;
        coords = v.ir.CompositeConstruct(v.F(base_reg), v.F(base_reg + 1));
        num_derivates = 2;
        break;
    }
    case TextureType::ARRAY_2D: {
        last_reg = base_reg + 2;
        coords = v.ir.CompositeConstruct(v.F(base_reg), v.F(base_reg + 1), read_array());
        num_derivates = 2;
        break;
    }
    default:
        throw NotImplementedException("Invalid texture type");
    }

    const IR::Reg derivate_reg{txd.derivate_reg};
    IR::Value derivates;
    switch (num_derivates) {
    case 1: {
        derivates = v.ir.CompositeConstruct(v.F(derivate_reg), v.F(derivate_reg + 1));
        break;
    }
    case 2: {
        derivates = v.ir.CompositeConstruct(v.F(derivate_reg), v.F(derivate_reg + 1),
                                            v.F(derivate_reg + 2), v.F(derivate_reg + 3));
        break;
    }
    default:
        throw NotImplementedException("Invalid texture type");
    }

    IR::Value offset;
    if (txd.aoffi != 0) {
        offset = MakeOffset(v, last_reg, has_lod_clamp);
    }

    IR::F32 lod_clamp;
    if (has_lod_clamp) {
        // Lod Clamp is a Fixed Point 4.8, we need to transform it to float.
        // to convert a fixed point, float(value) / float(1 << fixed_point)
        // in this case the fixed_point is 8.
        const IR::F32 conv4_8fixp_f{v.ir.Imm32(static_cast<f32>(1U << 8))};
        const IR::F32 fixp_lc{v.ir.ConvertUToF(
            32, 16, v.ir.BitFieldExtract(v.X(last_reg), v.ir.Imm32(20), v.ir.Imm32(12)))};
        lod_clamp = v.ir.FPMul(fixp_lc, conv4_8fixp_f);
    }

    IR::TextureInstInfo info{};
    info.type.Assign(GetType(txd.type));
    info.num_derivates.Assign(num_derivates);
    info.has_lod_clamp.Assign(has_lod_clamp ? 1 : 0);
    const IR::Value sample{v.ir.ImageGradient(handle, coords, derivates, offset, lod_clamp, info)};

    IR::Reg dest_reg{txd.dest_reg};
    for (size_t element = 0; element < 4; ++element) {
        if (((txd.mask >> element) & 1) == 0) {
            continue;
        }
        v.F(dest_reg, IR::F32{v.ir.CompositeExtract(sample, element)});
        ++dest_reg;
    }
    if (txd.sparse_pred != IR::Pred::PT) {
        v.ir.SetPred(txd.sparse_pred, v.ir.LogicalNot(v.ir.GetSparseFromOp(sample)));
    }
}
} // Anonymous namespace

void TranslatorVisitor::TXD(u64 insn) {
    Impl(*this, insn, false);
}

void TranslatorVisitor::TXD_b(u64 insn) {
    Impl(*this, insn, true);
}

} // namespace Shader::Maxwell
