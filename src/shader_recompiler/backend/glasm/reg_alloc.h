// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <bitset>

#include "common/common_types.h"

namespace Shader::IR {
class Inst;
class Value;
} // namespace Shader::IR

namespace Shader::Backend::GLASM {

class EmitContext;

struct Id {
    u32 index : 30;
    u32 is_spill : 1;
    u32 is_condition_code : 1;
};

class RegAlloc {
public:
    RegAlloc(EmitContext& ctx_) : ctx{ctx_} {}

    std::string Define(IR::Inst& inst);

    std::string Consume(const IR::Value& value);

    [[nodiscard]] size_t NumUsedRegisters() const noexcept {
        return num_used_registers;
    }

private:
    static constexpr size_t NUM_REGS = 4096;
    static constexpr size_t NUM_ELEMENTS = 4;

    EmitContext& ctx;

    std::string Consume(IR::Inst& inst);

    Id Alloc();

    void Free(Id id);

    size_t num_used_registers{};
    std::bitset<NUM_REGS> register_use{};
};

} // namespace Shader::Backend::GLASM
