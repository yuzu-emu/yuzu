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
enum class Mode : u64 {
    Dimension = 1,
    TextureType = 2,
    SamplePos = 5,
};

IR::Value Query(TranslatorVisitor& v, const IR::U32& handle, Mode mode, IR::Reg src_reg) {
    switch (mode) {
    case Mode::Dimension: {
        const IR::U32 lod{v.X(src_reg)};
        return v.ir.ImageQueryDimension(handle, lod);
    }
    case Mode::TextureType:
    case Mode::SamplePos:
    default:
        throw NotImplementedException("Mode {}", mode);
    }
}

void Impl(TranslatorVisitor& v, u64 insn, std::optional<u32> cbuf_offset) {
    union {
        u64 raw;
        BitField<49, 1, u64> nodep;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_reg;
        BitField<22, 3, Mode> mode;
        BitField<31, 4, u64> mask;
    } const txq{insn};

    IR::Reg src_reg{txq.src_reg};
    IR::U32 handle;
    if (cbuf_offset) {
        handle = v.ir.Imm32(*cbuf_offset);
    } else {
        handle = v.X(src_reg);
        ++src_reg;
    }
    const IR::Value query{Query(v, handle, txq.mode, src_reg)};
    IR::Reg dest_reg{txq.dest_reg};
    for (int element = 0; element < 4; ++element) {
        if (((txq.mask >> element) & 1) == 0) {
            continue;
        }
        v.X(dest_reg, IR::U32{v.ir.CompositeExtract(query, static_cast<size_t>(element))});
        ++dest_reg;
    }
}
} // Anonymous namespace

void TranslatorVisitor::TXQ(u64 insn) {
    union {
        u64 raw;
        BitField<36, 13, u64> cbuf_offset;
    } const txq{insn};

    Impl(*this, insn, static_cast<u32>(txq.cbuf_offset * 4));
}

void TranslatorVisitor::TXQ_b(u64 insn) {
    Impl(*this, insn, std::nullopt);
}

} // namespace Shader::Maxwell
