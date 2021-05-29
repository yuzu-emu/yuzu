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

constexpr unsigned R = 1 << 0;
constexpr unsigned G = 1 << 1;
constexpr unsigned B = 1 << 2;
constexpr unsigned A = 1 << 3;

constexpr std::array MASK{
    0U,            //
    R,             //
    G,             //
    R | G,         //
    B,             //
    R | B,         //
    G | B,         //
    R | G | B,     //
    A,             //
    R | A,         //
    G | A,         //
    R | G | A,     //
    B | A,         //
    R | B | A,     //
    G | B | A,     //
    R | G | B | A, //
};

enum class Size : u64 {
    U8,
    S8,
    U16,
    S16,
    B32,
    B64,
    B128,
};

enum class Clamp : u64 {
    IGN,
    Default,
    TRAP,
};

// https://docs.nvidia.com/cuda/parallel-thread-execution/index.html#cache-operators
enum class LoadCache : u64 {
    CA, // Cache at all levels, likely to be accessed again
    CG, // Cache at global level (L2 and below, not L1)
    CI, // ???
    CV, // Don't cache and fetch again (volatile)
};

enum class StoreCache : u64 {
    WB, // Cache write-back all coherent levels
    CG, // Cache at global level (L2 and below, not L1)
    CS, // Cache streaming, likely to be accessed once
    WT, // Cache write-through (to system memory, volatile?)
};

ImageFormat Format(Size size) {
    switch (size) {
    case Size::U8:
        return ImageFormat::R8_UINT;
    case Size::S8:
        return ImageFormat::R8_SINT;
    case Size::U16:
        return ImageFormat::R16_UINT;
    case Size::S16:
        return ImageFormat::R16_SINT;
    case Size::B32:
        return ImageFormat::R32_UINT;
    case Size::B64:
        return ImageFormat::R32G32_UINT;
    case Size::B128:
        return ImageFormat::R32G32B32A32_UINT;
    }
    throw NotImplementedException("Invalid size {}", size);
}

int SizeInRegs(Size size) {
    switch (size) {
    case Size::U8:
    case Size::S8:
    case Size::U16:
    case Size::S16:
    case Size::B32:
        return 1;
    case Size::B64:
        return 2;
    case Size::B128:
        return 4;
    }
    throw NotImplementedException("Invalid size {}", size);
}

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
    case Type::ARRAY_1D:
        return v.ir.CompositeConstruct(v.X(reg), array(1));
    case Type::_2D:
        return v.ir.CompositeConstruct(v.X(reg), v.X(reg + 1));
    case Type::ARRAY_2D:
        return v.ir.CompositeConstruct(v.X(reg), v.X(reg + 1), array(2));
    case Type::_3D:
        return v.ir.CompositeConstruct(v.X(reg), v.X(reg + 1), v.X(reg + 2));
    }
    throw NotImplementedException("Invalid type {}", type);
}

unsigned SwizzleMask(u64 swizzle) {
    if (swizzle == 0 || swizzle >= MASK.size()) {
        throw NotImplementedException("Invalid swizzle {}", swizzle);
    }
    return MASK[swizzle];
}

IR::Value MakeColor(IR::IREmitter& ir, IR::Reg reg, int num_regs) {
    std::array<IR::U32, 4> colors;
    for (int i = 0; i < num_regs; ++i) {
        colors[i] = ir.GetReg(reg + i);
    }
    for (int i = num_regs; i < 4; ++i) {
        colors[i] = ir.Imm32(0);
    }
    return ir.CompositeConstruct(colors[0], colors[1], colors[2], colors[3]);
}
} // Anonymous namespace

void TranslatorVisitor::SULD(u64 insn) {
    union {
        u64 raw;
        BitField<51, 1, u64> is_bound;
        BitField<52, 1, u64> d;
        BitField<23, 1, u64> ba;
        BitField<33, 3, Type> type;
        BitField<24, 2, LoadCache> cache;
        BitField<20, 3, Size> size;   // .D
        BitField<20, 4, u64> swizzle; // .P
        BitField<49, 2, Clamp> clamp;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> coord_reg;
        BitField<36, 13, u64> bound_offset;    // is_bound
        BitField<39, 8, IR::Reg> bindless_reg; // !is_bound
    } const suld{insn};

    if (suld.clamp != Clamp::IGN) {
        throw NotImplementedException("Clamp {}", suld.clamp.Value());
    }
    if (suld.cache != LoadCache::CA && suld.cache != LoadCache::CG) {
        throw NotImplementedException("Cache {}", suld.cache.Value());
    }
    const bool is_typed{suld.d != 0};
    if (is_typed && suld.ba != 0) {
        throw NotImplementedException("BA");
    }

    const ImageFormat format{is_typed ? Format(suld.size) : ImageFormat::Typeless};
    const TextureType type{GetType(suld.type)};
    const IR::Value coords{MakeCoords(*this, suld.coord_reg, suld.type)};
    const IR::U32 handle{suld.is_bound != 0 ? ir.Imm32(static_cast<u32>(suld.bound_offset * 4))
                                            : X(suld.bindless_reg)};
    IR::TextureInstInfo info{};
    info.type.Assign(type);
    info.image_format.Assign(format);

    const IR::Value result{ir.ImageRead(handle, coords, info)};
    IR::Reg dest_reg{suld.dest_reg};
    if (is_typed) {
        const int num_regs{SizeInRegs(suld.size)};
        for (int i = 0; i < num_regs; ++i) {
            X(dest_reg + i, IR::U32{ir.CompositeExtract(result, i)});
        }
    } else {
        const unsigned mask{SwizzleMask(suld.swizzle)};
        const int bits{std::popcount(mask)};
        if (!IR::IsAligned(dest_reg, bits == 3 ? 4 : bits)) {
            throw NotImplementedException("Unaligned destination register");
        }
        for (unsigned component = 0; component < 4; ++component) {
            if (((mask >> component) & 1) == 0) {
                continue;
            }
            X(dest_reg, IR::U32{ir.CompositeExtract(result, component)});
            ++dest_reg;
        }
    }
}

void TranslatorVisitor::SUST(u64 insn) {
    union {
        u64 raw;
        BitField<51, 1, u64> is_bound;
        BitField<52, 1, u64> d;
        BitField<23, 1, u64> ba;
        BitField<33, 3, Type> type;
        BitField<24, 2, StoreCache> cache;
        BitField<20, 3, Size> size;   // .D
        BitField<20, 4, u64> swizzle; // .P
        BitField<49, 2, Clamp> clamp;
        BitField<0, 8, IR::Reg> data_reg;
        BitField<8, 8, IR::Reg> coord_reg;
        BitField<36, 13, u64> bound_offset;    // is_bound
        BitField<39, 8, IR::Reg> bindless_reg; // !is_bound
    } const sust{insn};

    if (sust.clamp != Clamp::IGN) {
        throw NotImplementedException("Clamp {}", sust.clamp.Value());
    }
    if (sust.cache != StoreCache::WB && sust.cache != StoreCache::CG) {
        throw NotImplementedException("Cache {}", sust.cache.Value());
    }
    const bool is_typed{sust.d != 0};
    if (is_typed && sust.ba != 0) {
        throw NotImplementedException("BA");
    }
    const ImageFormat format{is_typed ? Format(sust.size) : ImageFormat::Typeless};
    const TextureType type{GetType(sust.type)};
    const IR::Value coords{MakeCoords(*this, sust.coord_reg, sust.type)};
    const IR::U32 handle{sust.is_bound != 0 ? ir.Imm32(static_cast<u32>(sust.bound_offset * 4))
                                            : X(sust.bindless_reg)};
    IR::TextureInstInfo info{};
    info.type.Assign(type);
    info.image_format.Assign(format);

    IR::Value color;
    if (is_typed) {
        color = MakeColor(ir, sust.data_reg, SizeInRegs(sust.size));
    } else {
        const unsigned mask{SwizzleMask(sust.swizzle)};
        if (mask != 0xf) {
            throw NotImplementedException("Non-full mask");
        }
        color = MakeColor(ir, sust.data_reg, 4);
    }
    ir.ImageWrite(handle, coords, color, info);
}

} // namespace Shader::Maxwell
