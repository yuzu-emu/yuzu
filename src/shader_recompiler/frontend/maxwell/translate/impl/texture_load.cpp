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

IR::Value MakeCoords(TranslatorVisitor& v, IR::Reg reg, TextureType type) {
    const auto read_array{
        [&]() -> IR::U32 { return v.ir.BitFieldExtract(v.X(reg), v.ir.Imm32(0), v.ir.Imm32(16)); }};
    switch (type) {
    case TextureType::_1D:
        return v.X(reg);
    case TextureType::ARRAY_1D:
        return v.ir.CompositeConstruct(v.X(reg + 1), read_array());
    case TextureType::_2D:
        return v.ir.CompositeConstruct(v.X(reg), v.X(reg + 1));
    case TextureType::ARRAY_2D:
        return v.ir.CompositeConstruct(v.X(reg + 1), v.X(reg + 2), read_array());
    case TextureType::_3D:
        return v.ir.CompositeConstruct(v.X(reg), v.X(reg + 1), v.X(reg + 2));
    case TextureType::ARRAY_3D:
        throw NotImplementedException("3D array texture type");
    case TextureType::CUBE:
        return v.ir.CompositeConstruct(v.X(reg), v.X(reg + 1), v.X(reg + 2));
    case TextureType::ARRAY_CUBE:
        return v.ir.CompositeConstruct(v.X(reg + 1), v.X(reg + 2), v.X(reg + 3), read_array());
    }
    throw NotImplementedException("Invalid texture type {}", type);
}

IR::Value MakeOffset(TranslatorVisitor& v, IR::Reg& reg, TextureType type) {
    const IR::U32 value{v.X(reg++)};
    switch (type) {
    case TextureType::_1D:
    case TextureType::ARRAY_1D:
        return v.ir.BitFieldExtract(value, v.ir.Imm32(0), v.ir.Imm32(4), true);
    case TextureType::_2D:
    case TextureType::ARRAY_2D:
        return v.ir.CompositeConstruct(
            v.ir.BitFieldExtract(value, v.ir.Imm32(0), v.ir.Imm32(4), true),
            v.ir.BitFieldExtract(value, v.ir.Imm32(4), v.ir.Imm32(4), true));
    case TextureType::_3D:
    case TextureType::ARRAY_3D:
        return v.ir.CompositeConstruct(
            v.ir.BitFieldExtract(value, v.ir.Imm32(0), v.ir.Imm32(4), true),
            v.ir.BitFieldExtract(value, v.ir.Imm32(4), v.ir.Imm32(4), true),
            v.ir.BitFieldExtract(value, v.ir.Imm32(8), v.ir.Imm32(4), true));
    case TextureType::CUBE:
    case TextureType::ARRAY_CUBE:
        throw NotImplementedException("Illegal offset on CUBE sample");
    }
    throw NotImplementedException("Invalid texture type {}", type);
}

void Impl(TranslatorVisitor& v, u64 insn, bool is_bindless) {
    union {
        u64 raw;
        BitField<49, 1, u64> nodep;
        BitField<55, 1, u64> lod;
        BitField<50, 1, u64> multisample;
        BitField<35, 1, u64> aoffi;
        BitField<54, 1, u64> clamp;
        BitField<51, 3, IR::Pred> sparse_pred;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> coord_reg;
        BitField<20, 8, IR::Reg> meta_reg;
        BitField<28, 3, TextureType> type;
        BitField<31, 4, u64> mask;
        BitField<36, 13, u64> cbuf_offset;
    } const tld{insn};

    const IR::Value coords{MakeCoords(v, tld.coord_reg, tld.type)};

    IR::Reg meta_reg{tld.meta_reg};
    IR::Value handle;
    IR::Value offset;
    IR::U32 lod;
    IR::U32 multisample;
    if (is_bindless) {
        handle = v.X(meta_reg++);
    } else {
        handle = v.ir.Imm32(static_cast<u32>(tld.cbuf_offset.Value() * 4));
    }
    if (tld.lod != 0) {
        lod = v.X(meta_reg++);
    } else {
        lod = v.ir.Imm32(0U);
    }
    if (tld.aoffi != 0) {
        offset = MakeOffset(v, meta_reg, tld.type);
    }
    if (tld.multisample != 0) {
        multisample = v.X(meta_reg++);
    }
    if (tld.clamp != 0) {
        throw NotImplementedException("TLD.CL - CLAMP is not implmented");
    }
    IR::TextureInstInfo info{};
    info.type.Assign(GetType(tld.type));
    const IR::Value sample{v.ir.ImageFetch(handle, coords, offset, lod, multisample, info)};

    IR::Reg dest_reg{tld.dest_reg};
    for (size_t element = 0; element < 4; ++element) {
        if (((tld.mask >> element) & 1) == 0) {
            continue;
        }
        v.F(dest_reg, IR::F32{v.ir.CompositeExtract(sample, element)});
        ++dest_reg;
    }
    if (tld.sparse_pred != IR::Pred::PT) {
        v.ir.SetPred(tld.sparse_pred, v.ir.LogicalNot(v.ir.GetSparseFromOp(sample)));
    }
}
} // Anonymous namespace

void TranslatorVisitor::TLD(u64 insn) {
    Impl(*this, insn, false);
}

void TranslatorVisitor::TLD_b(u64 insn) {
    Impl(*this, insn, true);
}

} // namespace Shader::Maxwell
