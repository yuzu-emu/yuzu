// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class Mode : u64 {
    Default,
    IL,
    IS,
    ISL,
};

enum class Size : u64 {
    U8,
    S8,
    U16,
    S16,
    B32,
    B64,
};

std::pair<IR::U32, IR::U32> Slot(IR::IREmitter& ir, Mode mode, const IR::U32& imm_index,
                                 const IR::U32& reg, const IR::U32& imm) {
    switch (mode) {
    case Mode::Default:
        return {imm_index, ir.IAdd(reg, imm)};
    default:
        break;
    }
    throw NotImplementedException("Mode {}", mode);
}
} // Anonymous namespace

void TranslatorVisitor::LDC(u64 insn) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_reg;
        BitField<20, 16, s64> offset;
        BitField<36, 5, u64> index;
        BitField<44, 2, Mode> mode;
        BitField<48, 3, Size> size;
    } const ldc{insn};

    const IR::U32 imm_index{ir.Imm32(static_cast<u32>(ldc.index))};
    const IR::U32 reg{X(ldc.src_reg)};
    const IR::U32 imm{ir.Imm32(static_cast<s32>(ldc.offset))};
    const auto [index, offset]{Slot(ir, ldc.mode, imm_index, reg, imm)};
    switch (ldc.size) {
    case Size::U8:
        X(ldc.dest_reg, ir.GetCbuf(index, offset, 8, false));
        break;
    case Size::S8:
        X(ldc.dest_reg, ir.GetCbuf(index, offset, 8, true));
        break;
    case Size::U16:
        X(ldc.dest_reg, ir.GetCbuf(index, offset, 16, false));
        break;
    case Size::S16:
        X(ldc.dest_reg, ir.GetCbuf(index, offset, 16, true));
        break;
    case Size::B32:
        X(ldc.dest_reg, ir.GetCbuf(index, offset, 32, false));
        break;
    case Size::B64: {
        if (!IR::IsAligned(ldc.dest_reg, 2)) {
            throw NotImplementedException("Unaligned destination register");
        }
        const IR::Value vector{ir.UnpackUint2x32(ir.GetCbuf(index, offset, 64, false))};
        for (int i = 0; i < 2; ++i) {
            X(ldc.dest_reg + i, IR::U32{ir.CompositeExtract(vector, i)});
        }
        break;
    }
    default:
        throw NotImplementedException("Invalid size {}", ldc.size.Value());
    }
}

} // namespace Shader::Maxwell
