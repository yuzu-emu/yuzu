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
    // The ISA reads an array component here, but this is not needed on high level shading languages
    // We are dropping this information.
    switch (type) {
    case TextureType::_1D:
        return v.F(reg);
    case TextureType::ARRAY_1D:
        return v.F(reg + 1);
    case TextureType::_2D:
        return v.ir.CompositeConstruct(v.F(reg), v.F(reg + 1));
    case TextureType::ARRAY_2D:
        return v.ir.CompositeConstruct(v.F(reg + 1), v.F(reg + 2));
    case TextureType::_3D:
        return v.ir.CompositeConstruct(v.F(reg), v.F(reg + 1), v.F(reg + 2));
    case TextureType::ARRAY_3D:
        throw NotImplementedException("3D array texture type");
    case TextureType::CUBE:
        return v.ir.CompositeConstruct(v.F(reg), v.F(reg + 1), v.F(reg + 2));
    case TextureType::ARRAY_CUBE:
        return v.ir.CompositeConstruct(v.F(reg + 1), v.F(reg + 2), v.F(reg + 3));
    }
    throw NotImplementedException("Invalid texture type {}", type);
}

void Impl(TranslatorVisitor& v, u64 insn, bool is_bindless) {
    union {
        u64 raw;
        BitField<49, 1, u64> nodep;
        BitField<35, 1, u64> ndv;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> coord_reg;
        BitField<20, 8, IR::Reg> meta_reg;
        BitField<28, 3, TextureType> type;
        BitField<31, 4, u64> mask;
        BitField<36, 13, u64> cbuf_offset;
    } const tmml{insn};

    if ((tmml.mask & 0b1100) != 0) {
        throw NotImplementedException("TMML BA results are not implmented");
    }
    const IR::Value coords{MakeCoords(v, tmml.coord_reg, tmml.type)};

    IR::U32 handle;
    IR::Reg meta_reg{tmml.meta_reg};
    if (is_bindless) {
        handle = v.X(meta_reg++);
    } else {
        handle = v.ir.Imm32(static_cast<u32>(tmml.cbuf_offset.Value() * 4));
    }
    IR::TextureInstInfo info{};
    info.type.Assign(GetType(tmml.type));
    const IR::Value sample{v.ir.ImageQueryLod(handle, coords, info)};

    IR::Reg dest_reg{tmml.dest_reg};
    for (size_t element = 0; element < 4; ++element) {
        if (((tmml.mask >> element) & 1) == 0) {
            continue;
        }
        IR::F32 value{v.ir.CompositeExtract(sample, element)};
        if (element < 2) {
            IR::U32 casted_value;
            if (element == 0) {
                casted_value = v.ir.ConvertFToU(32, value);
            } else {
                casted_value = v.ir.ConvertFToS(16, value);
            }
            v.X(dest_reg, v.ir.ShiftLeftLogical(casted_value, v.ir.Imm32(8)));
        } else {
            v.F(dest_reg, value);
        }
        ++dest_reg;
    }
}
} // Anonymous namespace

void TranslatorVisitor::TMML(u64 insn) {
    Impl(*this, insn, false);
}

void TranslatorVisitor::TMML_b(u64 insn) {
    Impl(*this, insn, true);
}

} // namespace Shader::Maxwell
