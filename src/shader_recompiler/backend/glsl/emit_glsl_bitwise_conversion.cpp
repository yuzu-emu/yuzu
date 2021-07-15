// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLSL {
namespace {
static void Alias(IR::Inst& inst, const IR::Value& value) {
    if (value.IsImmediate()) {
        return;
    }
    IR::Inst& value_inst{*value.InstRecursive()};
    value_inst.DestructiveAddUsage(inst.UseCount());
    value_inst.DestructiveRemoveUsage();
    inst.SetDefinition(value_inst.Definition<Id>());
}
} // namespace

void EmitIdentity(EmitContext&, IR::Inst& inst, const IR::Value& value) {
    Alias(inst, value);
}

void EmitConditionRef(EmitContext& ctx, IR::Inst& inst, const IR::Value& value) {
    ctx.AddU1("{}={};", inst, ctx.reg_alloc.Consume(value));
}

void EmitBitCastU16F16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBitCastU32F32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU32("{}=floatBitsToUint({});", inst, value);
}

void EmitBitCastU64F64(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU64("{}=doubleBitsToUint64({});", inst, value);
}

void EmitBitCastF16U16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBitCastF32U32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF32("{}=uintBitsToFloat({});", inst, value);
}

void EmitBitCastF64U64(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF64("{}=uint64BitsToDouble({});", inst, value);
}

void EmitPackUint2x32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU64("{}=packUint2x32({});", inst, value);
}

void EmitUnpackUint2x32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU32x2("{}=unpackUint2x32({});", inst, value);
}

void EmitPackFloat2x16(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU32("{}=packFloat2x16({});", inst, value);
}

void EmitUnpackFloat2x16(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF16x2("{}=unpackFloat2x16({});", inst, value);
}

void EmitPackHalf2x16(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU32("{}=packHalf2x16({});", inst, value);
}

void EmitUnpackHalf2x16(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF32x2("{}=unpackHalf2x16({});", inst, value);
}

void EmitPackDouble2x32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF64("{}=packDouble2x32({});", inst, value);
}

void EmitUnpackDouble2x32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU32x2("{}=unpackDouble2x32({});", inst, value);
}

} // namespace Shader::Backend::GLSL
