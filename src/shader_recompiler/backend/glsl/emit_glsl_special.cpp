// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::GLSL {
namespace {
void InitializeOutputVaryings(EmitContext& ctx) {
    if (ctx.stage == Stage::VertexB || ctx.stage == Stage::Geometry) {
        ctx.Add("gl_Position=vec4(0,0,0,1);");
    }
    for (size_t index = 0; index < 16; ++index) {
        if (ctx.info.stores_generics[index]) {
            ctx.Add("out_attr{}=vec4(0,0,0,1);", index);
        }
    }
}
} // Anonymous namespace

void EmitPhi(EmitContext& ctx, IR::Inst& phi) {
    const size_t num_args{phi.NumArgs()};
    for (size_t i = 0; i < num_args; ++i) {
        ctx.var_alloc.Consume(phi.Arg(i));
    }
    if (!phi.Definition<Id>().is_valid) {
        // The phi node wasn't forward defined
        ctx.var_alloc.PhiDefine(phi, phi.Arg(0).Type());
    }
}

void EmitVoid(EmitContext&) {}

void EmitReference(EmitContext& ctx, const IR::Value& value) {
    ctx.var_alloc.Consume(value);
}

void EmitPhiMove(EmitContext& ctx, const IR::Value& phi_value, const IR::Value& value) {
    IR::Inst& phi{*phi_value.InstRecursive()};
    const auto phi_type{phi.Arg(0).Type()};
    if (!phi.Definition<Id>().is_valid) {
        // The phi node wasn't forward defined
        ctx.var_alloc.PhiDefine(phi, phi_type);
    }
    const auto phi_reg{ctx.var_alloc.Consume(IR::Value{&phi})};
    const auto val_reg{ctx.var_alloc.Consume(value)};
    if (phi_reg == val_reg) {
        return;
    }
    ctx.Add("{}={};", phi_reg, val_reg);
}

void EmitPrologue(EmitContext& ctx) {
    InitializeOutputVaryings(ctx);

    if (ctx.stage == Stage::Fragment && ctx.profile.need_declared_frag_colors) {
        for (size_t index = 0; index < ctx.info.stores_frag_color.size(); ++index) {
            if (ctx.info.stores_frag_color[index]) {
                continue;
            }
            ctx.Add("frag_color{}=vec4(0,0,0,1);", index);
        }
    }
}

void EmitEpilogue(EmitContext&) {}

void EmitEmitVertex(EmitContext& ctx, const IR::Value& stream) {
    ctx.Add("EmitStreamVertex(int({}));", ctx.var_alloc.Consume(stream));
    InitializeOutputVaryings(ctx);
}

void EmitEndPrimitive(EmitContext& ctx, const IR::Value& stream) {
    ctx.Add("EndStreamPrimitive(int({}));", ctx.var_alloc.Consume(stream));
}

} // namespace Shader::Backend::GLSL
