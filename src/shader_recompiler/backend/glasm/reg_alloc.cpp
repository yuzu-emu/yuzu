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

Register RegAlloc::Define(IR::Inst& inst) {
    const Id id{Alloc()};
    inst.SetDefinition<Id>(id);
    Register ret;
    ret.type = Type::Register;
    ret.id = id;
    return ret;
}

Value RegAlloc::Consume(const IR::Value& value) {
    if (!value.IsImmediate()) {
        return Consume(*value.InstRecursive());
    }
    Value ret;
    switch (value.Type()) {
    case IR::Type::U1:
        ret.type = Type::U32;
        ret.imm_u32 = value.U1() ? 0xffffffff : 0;
        break;
    case IR::Type::U32:
        ret.type = Type::U32;
        ret.imm_u32 = value.U32();
        break;
    case IR::Type::F32:
        ret.type = Type::F32;
        ret.imm_f32 = value.F32();
        break;
    default:
        throw NotImplementedException("Immediate type {}", value.Type());
    }
    return ret;
}

Register RegAlloc::AllocReg() {
    Register ret;
    ret.type = Type::Register;
    ret.id = Alloc();
    return ret;
}

void RegAlloc::FreeReg(Register reg) {
    Free(reg.id);
}

Value RegAlloc::Consume(IR::Inst& inst) {
    const Id id{inst.Definition<Id>()};
    inst.DestructiveRemoveUsage();
    if (!inst.HasUses()) {
        Free(id);
    }
    Value ret;
    ret.type = Type::Register;
    ret.id = id;
    return ret;
}

Id RegAlloc::Alloc() {
    for (size_t reg = 0; reg < NUM_REGS; ++reg) {
        if (register_use[reg]) {
            continue;
        }
        num_used_registers = std::max(num_used_registers, reg + 1);
        register_use[reg] = true;
        Id ret{};
        ret.index.Assign(static_cast<u32>(reg));
        ret.is_spill.Assign(0);
        ret.is_condition_code.Assign(0);
        return ret;
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
