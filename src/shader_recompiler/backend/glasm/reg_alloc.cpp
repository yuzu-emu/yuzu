// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>
#include <string_view>

#include <fmt/format.h>

#include "shader_recompiler/backend/glasm/reg_alloc.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLASM {
namespace {
constexpr std::string_view SWIZZLE = "xyzw";

std::string Representation(Id id) {
    if (id.is_condition_code != 0) {
        throw NotImplementedException("Condition code");
    }
    if (id.is_spill != 0) {
        throw NotImplementedException("Spilling");
    }
    const u32 num_elements{id.num_elements_minus_one + 1};
    const u32 index{static_cast<u32>(id.index)};
    if (num_elements == 4) {
        return fmt::format("R{}", index);
    } else {
        return fmt::format("R{}.{}", index, SWIZZLE.substr(id.base_element, num_elements));
    }
}
} // Anonymous namespace

std::string RegAlloc::Define(IR::Inst& inst, u32 num_elements, u32 alignment) {
    const Id id{Alloc(num_elements, alignment)};
    inst.SetDefinition<Id>(id);
    return Representation(id);
}

std::string RegAlloc::Consume(const IR::Value& value) {
    if (!value.IsImmediate()) {
        return Consume(*value.Inst());
    }
    throw NotImplementedException("Immediate loading");
}

std::string RegAlloc::Consume(IR::Inst& inst) {
    const Id id{inst.Definition<Id>()};
    inst.DestructiveRemoveUsage();
    if (!inst.HasUses()) {
        Free(id);
    }
    return Representation(inst.Definition<Id>());
}

Id RegAlloc::Alloc(u32 num_elements, [[maybe_unused]] u32 alignment) {
    for (size_t reg = 0; reg < NUM_REGS; ++reg) {
        if (register_use[reg]) {
            continue;
        }
        num_used_registers = std::max(num_used_registers, reg + 1);
        register_use[reg] = true;
        return Id{
            .base_element = 0,
            .num_elements_minus_one = num_elements - 1,
            .index = static_cast<u32>(reg),
            .is_spill = 0,
            .is_condition_code = 0,
        };
    }
    throw NotImplementedException("Register spilling");
}

void RegAlloc::Free(Id id) {
    if (id.is_spill != 0) {
        throw NotImplementedException("Free spill");
    }
    register_use[id.index] = false;
}

} // namespace Shader::Backend::GLASM
