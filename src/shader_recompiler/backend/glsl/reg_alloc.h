// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <bitset>

#include "common/bit_field.h"
#include "common/common_types.h"

namespace Shader::IR {
class Inst;
class Value;
enum class Type;
} // namespace Shader::IR

namespace Shader::Backend::GLSL {
enum class Type : u32 {
    U1,
    S32,
    U32,
    F32,
    S64,
    U64,
    F64,
    U32x2,
    F32x2,
    Void,
};

struct Id {
    union {
        u32 raw;
        BitField<0, 29, u32> index;
        BitField<29, 1, u32> is_long;
        BitField<30, 1, u32> is_spill;
        BitField<31, 1, u32> is_condition_code;
    };

    bool operator==(Id rhs) const noexcept {
        return raw == rhs.raw;
    }
    bool operator!=(Id rhs) const noexcept {
        return !operator==(rhs);
    }
};
static_assert(sizeof(Id) == sizeof(u32));

class RegAlloc {
public:
    std::string Define(IR::Inst& inst);
    std::string Define(IR::Inst& inst, Type type);
    std::string Define(IR::Inst& inst, IR::Type type);

    std::string Consume(const IR::Value& value);

    /// Returns true if the instruction is expected to be aliased to another
    static bool IsAliased(const IR::Inst& inst);

    /// Returns the underlying value out of an alias sequence
    static IR::Inst& AliasInst(IR::Inst& inst);

private:
    static constexpr size_t NUM_REGS = 4096;
    static constexpr size_t NUM_ELEMENTS = 4;

    std::string Consume(IR::Inst& inst);
    std::string GetType(Type type, u32 index);

    Id Alloc();
    void Free(Id id);

    size_t num_used_registers{};
    std::bitset<NUM_REGS> register_use{};
    std::bitset<NUM_REGS> register_defined{};
};

} // namespace Shader::Backend::GLSL
