// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
void Check(u64 insn) {
    union {
        u64 raw;
        BitField<5, 1, u64> cbuf_mode;
        BitField<6, 1, u64> lmt;
    } const encoding{insn};

    if (encoding.cbuf_mode != 0) {
        throw NotImplementedException("Constant buffer mode");
    }
    if (encoding.lmt != 0) {
        throw NotImplementedException("LMT");
    }
}
} // Anonymous namespace

void TranslatorVisitor::BRX(u64 insn) {
    Check(insn);
}

void TranslatorVisitor::JMX(u64 insn) {
    Check(insn);
}

} // namespace Shader::Maxwell
