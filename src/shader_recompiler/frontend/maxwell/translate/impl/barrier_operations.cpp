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
    CTA,
    GL,
    SYS,
    VC,
};
} // Anonymous namespace

void TranslatorVisitor::MEMBAR(u64 inst) {
    union {
        u64 raw;
        BitField<8, 2, LocalScope> scope;
    } const membar{inst};

    if (membar.scope == LocalScope::CTA) {
        ir.WorkgroupMemoryBarrier();
    } else {
        ir.DeviceMemoryBarrier();
    }
}

void TranslatorVisitor::DEPBAR() {
    // DEPBAR is a no-op
}

void TranslatorVisitor::BAR(u64 insn) {
    enum class Mode {
        RedPopc,
        Scan,
        RedAnd,
        RedOr,
        Sync,
        Arrive,
    };
    union {
        u64 raw;
        BitField<43, 1, u64> is_a_imm;
        BitField<44, 1, u64> is_b_imm;
        BitField<8, 8, u64> imm_a;
        BitField<20, 12, u64> imm_b;
        BitField<42, 1, u64> neg_pred;
        BitField<39, 3, IR::Pred> pred;
    } const bar{insn};

    const Mode mode{[insn] {
        switch (insn & 0x0000009B00000000ULL) {
        case 0x0000000200000000ULL:
            return Mode::RedPopc;
        case 0x0000000300000000ULL:
            return Mode::Scan;
        case 0x0000000A00000000ULL:
            return Mode::RedAnd;
        case 0x0000001200000000ULL:
            return Mode::RedOr;
        case 0x0000008000000000ULL:
            return Mode::Sync;
        case 0x0000008100000000ULL:
            return Mode::Arrive;
        }
        throw NotImplementedException("Invalid encoding");
    }()};
    if (mode != Mode::Sync) {
        throw NotImplementedException("BAR mode {}", mode);
    }
    if (bar.is_a_imm == 0) {
        throw NotImplementedException("Non-immediate input A");
    }
    if (bar.imm_a != 0) {
        throw NotImplementedException("Non-zero input A");
    }
    if (bar.is_b_imm == 0) {
        throw NotImplementedException("Non-immediate input B");
    }
    if (bar.imm_b != 0) {
        throw NotImplementedException("Non-zero input B");
    }
    if (bar.pred != IR::Pred::PT && bar.neg_pred != 0) {
        throw NotImplementedException("Non-true input predicate");
    }
    ir.Barrier();
}

} // namespace Shader::Maxwell
