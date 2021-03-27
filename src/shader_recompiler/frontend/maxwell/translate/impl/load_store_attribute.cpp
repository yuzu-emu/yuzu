// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/frontend/maxwell/opcodes.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class Size : u64 {
    B32,
    B64,
    B96,
    B128,
};

enum class InterpolationMode : u64 {
    Pass,
    Multiply,
    Constant,
    Sc,
};

enum class SampleMode : u64 {
    Default,
    Centroid,
    Offset,
};

int NumElements(Size size) {
    switch (size) {
    case Size::B32:
        return 1;
    case Size::B64:
        return 2;
    case Size::B96:
        return 3;
    case Size::B128:
        return 4;
    }
    throw InvalidArgument("Invalid size {}", size);
}
} // Anonymous namespace

void TranslatorVisitor::ALD(u64 insn) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> index_reg;
        BitField<20, 10, u64> absolute_offset;
        BitField<20, 11, s64> relative_offset;
        BitField<39, 8, IR::Reg> stream_reg;
        BitField<32, 1, u64> o;
        BitField<31, 1, u64> patch;
        BitField<47, 2, Size> size;
    } const ald{insn};

    if (ald.o != 0) {
        throw NotImplementedException("O");
    }
    if (ald.patch != 0) {
        throw NotImplementedException("P");
    }
    if (ald.index_reg != IR::Reg::RZ) {
        throw NotImplementedException("Indexed");
    }
    const u64 offset{ald.absolute_offset.Value()};
    if (offset % 4 != 0) {
        throw NotImplementedException("Unaligned absolute offset {}", offset);
    }
    const int num_elements{NumElements(ald.size)};
    for (int element = 0; element < num_elements; ++element) {
        F(ald.dest_reg + element, ir.GetAttribute(IR::Attribute{offset / 4 + element}));
    }
}

void TranslatorVisitor::AST(u64 insn) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> src_reg;
        BitField<8, 8, IR::Reg> index_reg;
        BitField<20, 10, u64> absolute_offset;
        BitField<20, 11, s64> relative_offset;
        BitField<31, 1, u64> patch;
        BitField<39, 8, IR::Reg> stream_reg;
        BitField<47, 2, Size> size;
    } const ast{insn};

    if (ast.patch != 0) {
        throw NotImplementedException("P");
    }
    if (ast.stream_reg != IR::Reg::RZ) {
        throw NotImplementedException("Stream store");
    }
    if (ast.index_reg != IR::Reg::RZ) {
        throw NotImplementedException("Indexed store");
    }
    const u64 offset{ast.absolute_offset.Value()};
    if (offset % 4 != 0) {
        throw NotImplementedException("Unaligned absolute offset {}", offset);
    }
    const int num_elements{NumElements(ast.size)};
    for (int element = 0; element < num_elements; ++element) {
        ir.SetAttribute(IR::Attribute{offset / 4 + element}, F(ast.src_reg + element));
    }
}

void TranslatorVisitor::IPA(u64 insn) {
    // IPA is the instruction used to read varyings from a fragment shader.
    // gl_FragCoord is mapped to the gl_Position attribute.
    // It yields unknown results when used outside of the fragment shader stage.
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> index_reg;
        BitField<20, 8, IR::Reg> multiplier;
        BitField<30, 8, IR::Attribute> attribute;
        BitField<38, 1, u64> idx;
        BitField<51, 1, u64> sat;
        BitField<52, 2, SampleMode> sample_mode;
        BitField<54, 2, InterpolationMode> interpolation_mode;
    } const ipa{insn};

    // Indexed IPAs are used for indexed varyings.
    // For example:
    //
    // in vec4 colors[4];
    // uniform int idx;
    // void main() {
    //     gl_FragColor = colors[idx];
    // }
    const bool is_indexed{ipa.idx != 0 && ipa.index_reg != IR::Reg::RZ};
    if (is_indexed) {
        throw NotImplementedException("IDX");
    }

    const IR::Attribute attribute{ipa.attribute};
    IR::F32 value{ir.GetAttribute(attribute)};
    if (IR::IsGeneric(attribute)) {
        const ProgramHeader& sph{env.SPH()};
        const u32 attr_index{IR::GenericAttributeIndex(attribute)};
        const u32 element{static_cast<u32>(attribute) % 4};
        const std::array input_map{sph.ps.GenericInputMap(attr_index)};
        const bool is_perspective{input_map[element] == Shader::PixelImap::Perspective};
        if (is_perspective) {
            const IR::F32 position_w{ir.GetAttribute(IR::Attribute::PositionW)};
            value = ir.FPMul(value, position_w);
        }
    }
    if (ipa.interpolation_mode == InterpolationMode::Multiply) {
        value = ir.FPMul(value, F(ipa.multiplier));
    }

    // Saturated IPAs are generally generated out of clamped varyings.
    // For example: clamp(some_varying, 0.0, 1.0)
    const bool is_saturated{ipa.sat != 0};
    if (is_saturated) {
        if (attribute == IR::Attribute::FrontFace) {
            throw NotImplementedException("IPA.SAT on FrontFace");
        }
        value = ir.FPSaturate(value);
    }

    F(ipa.dest_reg, value);
}

} // namespace Shader::Maxwell
