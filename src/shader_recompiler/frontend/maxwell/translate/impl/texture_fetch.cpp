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
enum class Blod : u64 {
    None,
    LZ,
    LB,
    LL,
    INVALIDBLOD4,
    INVALIDBLOD5,
    LBA,
    LLA,
};

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
    const auto read_array{[&]() -> IR::F32 { return v.ir.ConvertUToF(32, 16, v.X(reg)); }};
    switch (type) {
    case TextureType::_1D:
        return v.F(reg);
    case TextureType::ARRAY_1D:
        return v.ir.CompositeConstruct(v.F(reg + 1), read_array());
    case TextureType::_2D:
        return v.ir.CompositeConstruct(v.F(reg), v.F(reg + 1));
    case TextureType::ARRAY_2D:
        return v.ir.CompositeConstruct(v.F(reg + 1), v.F(reg + 2), read_array());
    case TextureType::_3D:
        return v.ir.CompositeConstruct(v.F(reg), v.F(reg + 1), v.F(reg + 2));
    case TextureType::ARRAY_3D:
        throw NotImplementedException("3D array texture type");
    case TextureType::CUBE:
        return v.ir.CompositeConstruct(v.F(reg), v.F(reg + 1), v.F(reg + 2));
    case TextureType::ARRAY_CUBE:
        return v.ir.CompositeConstruct(v.F(reg + 1), v.F(reg + 2), v.F(reg + 3), read_array());
    }
    throw NotImplementedException("Invalid texture type {}", type);
}

IR::F32 MakeLod(TranslatorVisitor& v, IR::Reg& reg, Blod blod) {
    switch (blod) {
    case Blod::None:
        return v.ir.Imm32(0.0f);
    case Blod::LZ:
        return v.ir.Imm32(0.0f);
    case Blod::LB:
    case Blod::LL:
    case Blod::LBA:
    case Blod::LLA:
        return v.F(reg++);
    case Blod::INVALIDBLOD4:
    case Blod::INVALIDBLOD5:
        break;
    }
    throw NotImplementedException("Invalid blod {}", blod);
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

bool HasExplicitLod(Blod blod) {
    switch (blod) {
    case Blod::LL:
    case Blod::LLA:
    case Blod::LZ:
        return true;
    default:
        return false;
    }
}

void Impl(TranslatorVisitor& v, u64 insn, bool aoffi, Blod blod, bool lc,
          std::optional<u32> cbuf_offset) {
    union {
        u64 raw;
        BitField<35, 1, u64> ndv;
        BitField<49, 1, u64> nodep;
        BitField<50, 1, u64> dc;
        BitField<51, 3, IR::Pred> sparse_pred;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> coord_reg;
        BitField<20, 8, IR::Reg> meta_reg;
        BitField<28, 3, TextureType> type;
        BitField<31, 4, u64> mask;
    } const tex{insn};

    if (lc) {
        throw NotImplementedException("LC");
    }
    const IR::Value coords{MakeCoords(v, tex.coord_reg, tex.type)};

    IR::Reg meta_reg{tex.meta_reg};
    IR::Value handle;
    IR::Value offset;
    IR::F32 dref;
    IR::F32 lod_clamp;
    if (cbuf_offset) {
        handle = v.ir.Imm32(*cbuf_offset);
    } else {
        handle = v.X(meta_reg++);
    }
    const IR::F32 lod{MakeLod(v, meta_reg, blod)};
    if (aoffi) {
        offset = MakeOffset(v, meta_reg, tex.type);
    }
    if (tex.dc != 0) {
        dref = v.F(meta_reg++);
    }
    IR::TextureInstInfo info{};
    info.type.Assign(GetType(tex.type));
    info.is_depth.Assign(tex.dc != 0 ? 1 : 0);
    info.has_bias.Assign(blod == Blod::LB || blod == Blod::LBA ? 1 : 0);
    info.has_lod_clamp.Assign(lc ? 1 : 0);

    const IR::Value sample{[&]() -> IR::Value {
        if (tex.dc == 0) {
            if (HasExplicitLod(blod)) {
                return v.ir.ImageSampleExplicitLod(handle, coords, lod, offset, info);
            } else {
                return v.ir.ImageSampleImplicitLod(handle, coords, lod, offset, lod_clamp, info);
            }
        }
        if (HasExplicitLod(blod)) {
            return v.ir.ImageSampleDrefExplicitLod(handle, coords, dref, lod, offset, info);
        } else {
            return v.ir.ImageSampleDrefImplicitLod(handle, coords, dref, lod, offset, lod_clamp,
                                                   info);
        }
    }()};

    IR::Reg dest_reg{tex.dest_reg};
    for (int element = 0; element < 4; ++element) {
        if (((tex.mask >> element) & 1) == 0) {
            continue;
        }
        IR::F32 value;
        if (tex.dc != 0) {
            value = element < 3 ? IR::F32{sample} : v.ir.Imm32(1.0f);
        } else {
            value = IR::F32{v.ir.CompositeExtract(sample, static_cast<size_t>(element))};
        }
        v.F(dest_reg, value);
        ++dest_reg;
    }
    if (tex.sparse_pred != IR::Pred::PT) {
        v.ir.SetPred(tex.sparse_pred, v.ir.LogicalNot(v.ir.GetSparseFromOp(sample)));
    }
}
} // Anonymous namespace

void TranslatorVisitor::TEX(u64 insn) {
    union {
        u64 raw;
        BitField<54, 1, u64> aoffi;
        BitField<55, 3, Blod> blod;
        BitField<58, 1, u64> lc;
        BitField<36, 13, u64> cbuf_offset;
    } const tex{insn};

    Impl(*this, insn, tex.aoffi != 0, tex.blod, tex.lc != 0, static_cast<u32>(tex.cbuf_offset * 4));
}

void TranslatorVisitor::TEX_b(u64 insn) {
    union {
        u64 raw;
        BitField<36, 1, u64> aoffi;
        BitField<37, 3, Blod> blod;
        BitField<40, 1, u64> lc;
    } const tex{insn};

    Impl(*this, insn, tex.aoffi != 0, tex.blod, tex.lc != 0, std::nullopt);
}

} // namespace Shader::Maxwell
