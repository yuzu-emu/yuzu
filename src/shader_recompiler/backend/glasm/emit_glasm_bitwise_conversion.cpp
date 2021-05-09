// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLASM {

static void Alias(IR::Inst& inst, const IR::Value& value) {
    if (value.IsImmediate()) {
        return;
    }
    IR::Inst* const value_inst{value.InstRecursive()};
    if (inst.GetOpcode() == IR::Opcode::Identity) {
        value_inst->DestructiveAddUsage(inst.UseCount());
        value_inst->DestructiveRemoveUsage();
    }
    inst.SetDefinition(value_inst->Definition<Id>());
}

void EmitIdentity(EmitContext&, IR::Inst& inst, const IR::Value& value) {
    Alias(inst, value);
}

void EmitBitCastU16F16(EmitContext&, IR::Inst& inst, const IR::Value& value) {
    Alias(inst, value);
}

void EmitBitCastU32F32(EmitContext&, IR::Inst& inst, const IR::Value& value) {
    Alias(inst, value);
}

void EmitBitCastU64F64(EmitContext&, IR::Inst& inst, const IR::Value& value) {
    Alias(inst, value);
}

void EmitBitCastF16U16(EmitContext&, IR::Inst& inst, const IR::Value& value) {
    Alias(inst, value);
}

void EmitBitCastF32U32(EmitContext&, IR::Inst& inst, const IR::Value& value) {
    Alias(inst, value);
}

void EmitBitCastF64U64(EmitContext&, IR::Inst& inst, const IR::Value& value) {
    Alias(inst, value);
}

void EmitPackUint2x32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitUnpackUint2x32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitPackFloat2x16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitUnpackFloat2x16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitPackHalf2x16(EmitContext& ctx, IR::Inst& inst, Register value) {
    ctx.Add("PK2H {}.x,{};", inst, value);
}

void EmitUnpackHalf2x16(EmitContext& ctx, IR::Inst& inst, Register value) {
    ctx.Add("UP2H {}.xy,{}.x;", inst, value);
}

} // namespace Shader::Backend::GLASM
