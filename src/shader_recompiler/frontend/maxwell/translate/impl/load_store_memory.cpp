// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/maxwell/opcode.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class StoreSize : u64 {
    U8,
    S8,
    U16,
    S16,
    B32,
    B64,
    B128,
};

// See Table 28 in https://docs.nvidia.com/cuda/parallel-thread-execution/index.html
enum class StoreCache : u64 {
    WB, // Cache write-back all coherent levels
    CG, // Cache at global level
    CS, // Cache streaming, likely to be accessed once
    WT, // Cache write-through (to system memory)
};
} // Anonymous namespace

void TranslatorVisitor::STG(u64 insn) {
    // STG stores registers into global memory.
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> data_reg;
        BitField<8, 8, IR::Reg> addr_reg;
        BitField<45, 1, u64> e;
        BitField<46, 2, StoreCache> cache;
        BitField<48, 3, StoreSize> size;
    } const stg{insn};

    const IR::U64 address{[&]() -> IR::U64 {
        if (stg.e == 0) {
            // STG without .E uses a 32-bit pointer, zero-extend it
            return ir.ConvertU(64, X(stg.addr_reg));
        }
        if (!IR::IsAligned(stg.addr_reg, 2)) {
            throw NotImplementedException("Unaligned address register");
        }
        // Pack two registers to build the 32-bit address
        return ir.PackUint2x32(ir.CompositeConstruct(X(stg.addr_reg), X(stg.addr_reg + 1)));
    }()};

    switch (stg.size) {
    case StoreSize::U8:
        ir.WriteGlobalU8(address, X(stg.data_reg));
        break;
    case StoreSize::S8:
        ir.WriteGlobalS8(address, X(stg.data_reg));
        break;
    case StoreSize::U16:
        ir.WriteGlobalU16(address, X(stg.data_reg));
        break;
    case StoreSize::S16:
        ir.WriteGlobalS16(address, X(stg.data_reg));
        break;
    case StoreSize::B32:
        ir.WriteGlobal32(address, X(stg.data_reg));
        break;
    case StoreSize::B64: {
        if (!IR::IsAligned(stg.data_reg, 2)) {
            throw NotImplementedException("Unaligned data registers");
        }
        const IR::Value vector{ir.CompositeConstruct(X(stg.data_reg), X(stg.data_reg + 1))};
        ir.WriteGlobal64(address, vector);
        break;
    }
    case StoreSize::B128:
        if (!IR::IsAligned(stg.data_reg, 4)) {
            throw NotImplementedException("Unaligned data registers");
        }
        const IR::Value vector{ir.CompositeConstruct(X(stg.data_reg), X(stg.data_reg + 1),
                                                     X(stg.data_reg + 2), X(stg.data_reg + 3))};
        ir.WriteGlobal128(address, vector);
        break;
    }
}

} // namespace Shader::Maxwell
