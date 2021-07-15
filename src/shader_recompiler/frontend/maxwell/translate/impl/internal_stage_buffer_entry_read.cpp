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
    Patch,
    Prim,
    Attr,
};

enum class Shift : u64 {
    Default,
    U16,
    B32,
};

} // Anonymous namespace

void TranslatorVisitor::ISBERD(u64 insn) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_reg;
        BitField<31, 1, u64> skew;
        BitField<32, 1, u64> o;
        BitField<33, 2, Mode> mode;
        BitField<47, 2, Shift> shift;
    } const isberd{insn};

    if (isberd.skew != 0) {
        throw NotImplementedException("SKEW");
    }
    if (isberd.o != 0) {
        throw NotImplementedException("O");
    }
    if (isberd.mode != Mode::Default) {
        throw NotImplementedException("Mode {}", isberd.mode.Value());
    }
    if (isberd.shift != Shift::Default) {
        throw NotImplementedException("Shift {}", isberd.shift.Value());
    }
    LOG_WARNING(Shader, "(STUBBED) called");
    X(isberd.dest_reg, X(isberd.src_reg));
}

} // namespace Shader::Maxwell
