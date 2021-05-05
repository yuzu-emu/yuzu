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

struct Id {
    u32 base_element : 2;
    u32 num_elements_minus_one : 2;
    u32 index : 26;
    u32 is_spill : 1;
    u32 is_condition_code : 1;
};

class RegAlloc {
public:
    std::string Define(IR::Inst& inst, u32 num_elements = 1, u32 alignment = 1);

    std::string Consume(const IR::Value& value);

private:
    static constexpr size_t NUM_REGS = 4096;
    static constexpr size_t NUM_ELEMENTS = 4;

    std::string Consume(IR::Inst& inst);

    Id Alloc(u32 num_elements, u32 alignment);

    void Free(Id id);

    size_t num_used_registers{};
    std::bitset<NUM_REGS> register_use{};
};

} // namespace Shader::Backend::GLASM
