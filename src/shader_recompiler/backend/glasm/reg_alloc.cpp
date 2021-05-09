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
    return Define(inst, false);
}

Register RegAlloc::LongDefine(IR::Inst& inst) {
    return Define(inst, true);
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
    case IR::Type::F64:
        ret.type = Type::F64;
        ret.imm_f64 = value.F64();
        break;
    default:
        throw NotImplementedException("Immediate type {}", value.Type());
    }
    return ret;
}

Register RegAlloc::AllocReg() {
    Register ret;
    ret.type = Type::Register;
    ret.id = Alloc(false);
    return ret;
}

Register RegAlloc::AllocLongReg() {
    Register ret;
    ret.type = Type::Register;
    ret.id = Alloc(true);
    return ret;
}

void RegAlloc::FreeReg(Register reg) {
    Free(reg.id);
}

Register RegAlloc::Define(IR::Inst& inst, bool is_long) {
    const Id id{Alloc(is_long)};
    inst.SetDefinition<Id>(id);
    Register ret;
    ret.type = Type::Register;
    ret.id = id;
    return ret;
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

Id RegAlloc::Alloc(bool is_long) {
    size_t& num_regs{is_long ? num_used_long_registers : num_used_registers};
    std::bitset<NUM_REGS>& use{is_long ? long_register_use : register_use};
    if (num_used_registers + num_used_long_registers < NUM_REGS) {
        for (size_t reg = 0; reg < NUM_REGS; ++reg) {
            if (use[reg]) {
                continue;
            }
            num_regs = std::max(num_regs, reg + 1);
            use[reg] = true;
            Id ret{};
            ret.index.Assign(static_cast<u32>(reg));
            ret.is_long.Assign(is_long ? 1 : 0);
            ret.is_spill.Assign(0);
            ret.is_condition_code.Assign(0);
            return ret;
        }
    }
    throw NotImplementedException("Register spilling");
}

void RegAlloc::Free(Id id) {
    if (id.is_spill != 0) {
        throw NotImplementedException("Free spill");
    }
    if (id.is_long != 0) {
        long_register_use[id.index] = false;
    } else {
        register_use[id.index] = false;
    }
}

} // namespace Shader::Backend::GLASM
