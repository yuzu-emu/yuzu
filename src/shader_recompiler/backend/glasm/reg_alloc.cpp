// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>

#include <fmt/format.h>

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/reg_alloc.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLASM {
namespace {
std::string Representation(Id id) {
    if (id.is_condition_code != 0) {
        throw NotImplementedException("Condition code");
    }
    if (id.is_spill != 0) {
        throw NotImplementedException("Spilling");
    }
    const u32 index{static_cast<u32>(id.index)};
    return fmt::format("R{}.x", index);
}

std::string ImmValue(const IR::Value& value) {
    switch (value.Type()) {
    case IR::Type::U1:
        return value.U1() ? "-1" : "0";
    case IR::Type::U32:
        return fmt::format("{}", value.U32());
    case IR::Type::F32:
        return fmt::format("{}", value.F32());
    default:
        throw NotImplementedException("Immediate type", value.Type());
    }
}
} // Anonymous namespace

std::string RegAlloc::Define(IR::Inst& inst) {
    const Id id{Alloc()};
    inst.SetDefinition<Id>(id);
    return Representation(id);
}

std::string RegAlloc::Consume(const IR::Value& value) {
    if (value.IsImmediate()) {
        return ImmValue(value);
    } else {
        return Consume(*value.InstRecursive());
    }
}

std::string RegAlloc::Consume(IR::Inst& inst) {
    const Id id{inst.Definition<Id>()};
    inst.DestructiveRemoveUsage();
    if (!inst.HasUses()) {
        Free(id);
    }
    return Representation(inst.Definition<Id>());
}

Id RegAlloc::Alloc() {
    for (size_t reg = 0; reg < NUM_REGS; ++reg) {
        if (register_use[reg]) {
            continue;
        }
        num_used_registers = std::max(num_used_registers, reg + 1);
        register_use[reg] = true;
        return Id{
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
