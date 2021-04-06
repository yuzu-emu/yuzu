// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/maxwell/opcodes.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class LoadSize : u64 {
    U8,  // Zero-extend
    S8,  // Sign-extend
    U16, // Zero-extend
    S16, // Sign-extend
    B32,
    B64,
    B128,
    U128, // ???
};

enum class StoreSize : u64 {
    U8,  // Zero-extend
    S8,  // Sign-extend
    U16, // Zero-extend
    S16, // Sign-extend
    B32,
    B64,
    B128,
};

// See Table 27 in https://docs.nvidia.com/cuda/parallel-thread-execution/index.html
enum class LoadCache : u64 {
    CA, // Cache at all levels, likely to be accessed again
    CG, // Cache at global level (cache in L2 and below, not L1)
    CI, // ???
    CV, // Don't cache and fetch again (consider cached system memory lines stale, fetch again)
};

// See Table 28 in https://docs.nvidia.com/cuda/parallel-thread-execution/index.html
enum class StoreCache : u64 {
    WB, // Cache write-back all coherent levels
    CG, // Cache at global level
    CS, // Cache streaming, likely to be accessed once
    WT, // Cache write-through (to system memory)
};

IR::U64 Address(TranslatorVisitor& v, u64 insn) {
    union {
        u64 raw;
        BitField<8, 8, IR::Reg> addr_reg;
        BitField<20, 24, s64> addr_offset;
        BitField<20, 24, u64> rz_addr_offset;
        BitField<45, 1, u64> e;
    } const mem{insn};

    const IR::U64 address{[&]() -> IR::U64 {
        if (mem.e == 0) {
            // LDG/STG without .E uses a 32-bit pointer, zero-extend it
            return v.ir.UConvert(64, v.X(mem.addr_reg));
        }
        if (!IR::IsAligned(mem.addr_reg, 2)) {
            throw NotImplementedException("Unaligned address register");
        }
        // Pack two registers to build the 64-bit address
        return v.ir.PackUint2x32(v.ir.CompositeConstruct(v.X(mem.addr_reg), v.X(mem.addr_reg + 1)));
    }()};
    const u64 addr_offset{[&]() -> u64 {
        if (mem.addr_reg == IR::Reg::RZ) {
            // When RZ is used, the address is an absolute address
            return static_cast<u64>(mem.rz_addr_offset.Value());
        } else {
            return static_cast<u64>(mem.addr_offset.Value());
        }
    }()};
    // Apply the offset
    return v.ir.IAdd(address, v.ir.Imm64(addr_offset));
}
} // Anonymous namespace

void TranslatorVisitor::LDG(u64 insn) {
    // LDG loads global memory into registers
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<46, 2, LoadCache> cache;
        BitField<48, 3, LoadSize> size;
    } const ldg{insn};

    // Pointer to load data from
    const IR::U64 address{Address(*this, insn)};
    const IR::Reg dest_reg{ldg.dest_reg};
    switch (ldg.size) {
    case LoadSize::U8:
        X(dest_reg, ir.LoadGlobalU8(address));
        break;
    case LoadSize::S8:
        X(dest_reg, ir.LoadGlobalS8(address));
        break;
    case LoadSize::U16:
        X(dest_reg, ir.LoadGlobalU16(address));
        break;
    case LoadSize::S16:
        X(dest_reg, ir.LoadGlobalS16(address));
        break;
    case LoadSize::B32:
        X(dest_reg, ir.LoadGlobal32(address));
        break;
    case LoadSize::B64: {
        if (!IR::IsAligned(dest_reg, 2)) {
            throw NotImplementedException("Unaligned data registers");
        }
        const IR::Value vector{ir.LoadGlobal64(address)};
        for (int i = 0; i < 2; ++i) {
            X(dest_reg + i, IR::U32{ir.CompositeExtract(vector, static_cast<size_t>(i))});
        }
        break;
    }
    case LoadSize::B128:
    case LoadSize::U128: {
        if (!IR::IsAligned(dest_reg, 4)) {
            throw NotImplementedException("Unaligned data registers");
        }
        const IR::Value vector{ir.LoadGlobal128(address)};
        for (int i = 0; i < 4; ++i) {
            X(dest_reg + i, IR::U32{ir.CompositeExtract(vector, static_cast<size_t>(i))});
        }
        break;
    }
    default:
        throw NotImplementedException("Invalid LDG size {}", ldg.size.Value());
    }
}

void TranslatorVisitor::STG(u64 insn) {
    // STG stores registers into global memory.
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> data_reg;
        BitField<46, 2, StoreCache> cache;
        BitField<48, 3, StoreSize> size;
    } const stg{insn};

    // Pointer to store data into
    const IR::U64 address{Address(*this, insn)};
    const IR::Reg data_reg{stg.data_reg};
    switch (stg.size) {
    case StoreSize::U8:
        ir.WriteGlobalU8(address, X(data_reg));
        break;
    case StoreSize::S8:
        ir.WriteGlobalS8(address, X(data_reg));
        break;
    case StoreSize::U16:
        ir.WriteGlobalU16(address, X(data_reg));
        break;
    case StoreSize::S16:
        ir.WriteGlobalS16(address, X(data_reg));
        break;
    case StoreSize::B32:
        ir.WriteGlobal32(address, X(data_reg));
        break;
    case StoreSize::B64: {
        if (!IR::IsAligned(data_reg, 2)) {
            throw NotImplementedException("Unaligned data registers");
        }
        const IR::Value vector{ir.CompositeConstruct(X(data_reg), X(data_reg + 1))};
        ir.WriteGlobal64(address, vector);
        break;
    }
    case StoreSize::B128:
        if (!IR::IsAligned(data_reg, 4)) {
            throw NotImplementedException("Unaligned data registers");
        }
        const IR::Value vector{
            ir.CompositeConstruct(X(data_reg), X(data_reg + 1), X(data_reg + 2), X(data_reg + 3))};
        ir.WriteGlobal128(address, vector);
        break;
    }
}

} // namespace Shader::Maxwell
