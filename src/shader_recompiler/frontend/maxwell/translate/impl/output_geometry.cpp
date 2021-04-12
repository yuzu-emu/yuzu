// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
void OUT(TranslatorVisitor& v, u64 insn, IR::U32 stream_index) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> output_reg; // Not needed on host
        BitField<39, 1, u64> emit;
        BitField<40, 1, u64> cut;
    } const out{insn};

    stream_index = v.ir.BitwiseAnd(stream_index, v.ir.Imm32(0b11));

    if (out.emit != 0) {
        v.ir.EmitVertex(stream_index);
    }
    if (out.cut != 0) {
        v.ir.EndPrimitive(stream_index);
    }
    // Host doesn't need the output register, but we can write to it to avoid undefined reads
    v.X(out.dest_reg, v.ir.Imm32(0));
}
} // Anonymous namespace

void TranslatorVisitor::OUT_reg(u64 insn) {
    OUT(*this, insn, GetReg20(insn));
}

void TranslatorVisitor::OUT_cbuf(u64 insn) {
    OUT(*this, insn, GetCbuf(insn));
}

void TranslatorVisitor::OUT_imm(u64 insn) {
    OUT(*this, insn, GetImm20(insn));
}

} // namespace Shader::Maxwell
