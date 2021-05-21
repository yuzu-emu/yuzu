// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>
#include <string_view>

#include <fmt/format.h>

#include "shader_recompiler/backend/glsl/reg_alloc.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/ir/value.h"
#pragma optimize("", off)
namespace Shader::Backend::GLSL {
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
    return fmt::format("R{}", index);
}

std::string MakeImm(const IR::Value& value) {
    switch (value.Type()) {
    case IR::Type::U1:
        return fmt::format("{}", value.U1() ? "true" : "false");
    case IR::Type::U32:
        return fmt::format("{}", value.U32());
    case IR::Type::F32:
        return fmt::format("{}", value.F32());
    case IR::Type::U64:
        return fmt::format("{}", value.U64());
    case IR::Type::F64:
        return fmt::format("{}", value.F64());
    default:
        throw NotImplementedException("Immediate type {}", value.Type());
    }
}
} // Anonymous namespace

std::string RegAlloc::Define(IR::Inst& inst, u32 num_elements, u32 alignment) {
    const Id id{Alloc(num_elements, alignment)};
    inst.SetDefinition<Id>(id);
    return Representation(id);
}

std::string RegAlloc::Consume(const IR::Value& value) {
    const auto result = value.IsImmediate() ? MakeImm(value) : Consume(*value.InstRecursive());
    return result;
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

/*static*/ bool RegAlloc::IsAliased(const IR::Inst& inst) {
    switch (inst.GetOpcode()) {
    case IR::Opcode::Identity:
    case IR::Opcode::BitCastU16F16:
    case IR::Opcode::BitCastU32F32:
    case IR::Opcode::BitCastU64F64:
    case IR::Opcode::BitCastF16U16:
    case IR::Opcode::BitCastF32U32:
    case IR::Opcode::BitCastF64U64:
        return true;
    default:
        return false;
    }
}

/*static*/ IR::Inst& RegAlloc::AliasInst(IR::Inst& inst) {
    IR::Inst* it{&inst};
    while (IsAliased(*it)) {
        const IR::Value arg{it->Arg(0)};
        if (arg.IsImmediate()) {
            break;
        }
        it = arg.InstRecursive();
    }
    return *it;
}
} // namespace Shader::Backend::GLSL
