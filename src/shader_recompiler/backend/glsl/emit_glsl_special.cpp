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
std::string_view OutputVertexIndex(EmitContext& ctx) {
    return ctx.stage == Stage::TessellationControl ? "[gl_InvocationID]" : "";
}

void InitializeOutputVaryings(EmitContext& ctx) {
    if (ctx.uses_geometry_passthrough) {
        return;
    }
    if (ctx.stage == Stage::VertexB || ctx.stage == Stage::Geometry) {
        ctx.Add("gl_Position=vec4(0,0,0,1);");
    }
    for (size_t index = 0; index < IR::NUM_GENERICS; ++index) {
        if (!ctx.info.stores.Generic(index)) {
            continue;
        }
        const auto& info_array{ctx.output_generics.at(index)};
        const auto output_decorator{OutputVertexIndex(ctx)};
        size_t element{};
        while (element < info_array.size()) {
            const auto& info{info_array.at(element)};
            const auto varying_name{fmt::format("{}{}", info.name, output_decorator)};
            switch (info.num_components) {
            case 1: {
                const char value{element == 3 ? '1' : '0'};
                ctx.Add("{}={}.f;", varying_name, value);
                break;
            }
            case 2:
            case 3:
                if (element + info.num_components < 4) {
                    ctx.Add("{}=vec{}(0);", varying_name, info.num_components);
                } else {
                    // last element is the w component, must be initialized to 1
                    const auto zeros{info.num_components == 3 ? "0,0," : "0,"};
                    ctx.Add("{}=vec{}({}1);", varying_name, info.num_components, zeros);
                }
                break;
            case 4:
                ctx.Add("{}=vec4(0,0,0,1);", varying_name);
                break;
            default:
                break;
            }
            element += info.num_components;
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
