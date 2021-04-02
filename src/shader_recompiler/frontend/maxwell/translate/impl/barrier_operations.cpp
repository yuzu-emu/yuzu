// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/ir/modifiers.h"
#include "shader_recompiler/frontend/maxwell/opcodes.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
// Seems to be in CUDA terminology.
enum class LocalScope : u64 {
    CTG = 0,
    GL = 1,
    SYS = 2,
    VC = 3,
};

IR::MemoryScope LocalScopeToMemoryScope(LocalScope scope) {
    switch (scope) {
    case LocalScope::CTG:
        return IR::MemoryScope::Workgroup;
    case LocalScope::GL:
        return IR::MemoryScope::Device;
    case LocalScope::SYS:
        return IR::MemoryScope::System;
    default:
        throw NotImplementedException("Unimplemented Local Scope {}", scope);
    }
}

} // Anonymous namespace

void TranslatorVisitor::MEMBAR(u64 inst) {
    union {
        u64 raw;
        BitField<8, 2, LocalScope> scope;
    } membar{inst};
    ir.MemoryBarrier(LocalScopeToMemoryScope(membar.scope));
}

void TranslatorVisitor::DEPBAR() {
    // DEPBAR is a no-op
}

void TranslatorVisitor::BAR(u64) {
    throw NotImplementedException("Instruction {} is not implemented", Opcode::BAR);
}

} // namespace Shader::Maxwell
