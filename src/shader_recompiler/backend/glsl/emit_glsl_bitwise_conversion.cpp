// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::GLSL {
namespace {
static void Alias(IR::Inst& inst, const IR::Value& value) {
    if (value.IsImmediate()) {
        return;
    }
    IR::Inst& value_inst{RegAlloc::AliasInst(*value.Inst())};
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

void EmitPackUint2x32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU64("{}=packUint2x32({});", inst, value);
}

void EmitUnpackUint2x32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU32x2("{}=unpackUint2x32({});", inst, value);
}

} // namespace Shader::Backend::GLSL
