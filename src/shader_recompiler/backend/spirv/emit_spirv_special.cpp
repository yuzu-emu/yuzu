// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {
namespace {
void ConvertDepthMode(EmitContext& ctx) {
    const Id type{ctx.F32[1]};
    const Id position{ctx.OpLoad(ctx.F32[4], ctx.output_position)};
    const Id z{ctx.OpCompositeExtract(type, position, 2u)};
    const Id w{ctx.OpCompositeExtract(type, position, 3u)};
    const Id screen_depth{ctx.OpFMul(type, ctx.OpFAdd(type, z, w), ctx.Constant(type, 0.5f))};
    const Id vector{ctx.OpCompositeInsert(ctx.F32[4], screen_depth, position, 2u)};
    ctx.OpStore(ctx.output_position, vector);
}
} // Anonymous namespace

void EmitPrologue(EmitContext& ctx) {
    if (ctx.stage == Stage::VertexB) {
        const Id zero{ctx.Constant(ctx.F32[1], 0.0f)};
        const Id one{ctx.Constant(ctx.F32[1], 1.0f)};
        const Id default_vector{ctx.ConstantComposite(ctx.F32[4], zero, zero, zero, one)};
        ctx.OpStore(ctx.output_position, default_vector);
        for (const Id generic_id : ctx.output_generics) {
            if (Sirit::ValidId(generic_id)) {
                ctx.OpStore(generic_id, default_vector);
            }
        }
        if (ctx.profile.fixed_state_point_size) {
            const float point_size{*ctx.profile.fixed_state_point_size};
            ctx.OpStore(ctx.output_point_size, ctx.Constant(ctx.F32[1], point_size));
        }
    }
}

void EmitEpilogue(EmitContext& ctx) {
    if (ctx.stage == Stage::VertexB && ctx.profile.convert_depth_mode) {
        ConvertDepthMode(ctx);
    }
}

void EmitEmitVertex(EmitContext& ctx, const IR::Value& stream) {
    if (ctx.profile.convert_depth_mode) {
        ConvertDepthMode(ctx);
    }
    if (!stream.IsImmediate()) {
        // LOG_WARNING(..., "EmitVertex's stream is not constant");
        ctx.OpEmitStreamVertex(ctx.u32_zero_value);
        return;
    }
    ctx.OpEmitStreamVertex(ctx.Def(stream));
}

void EmitEndPrimitive(EmitContext& ctx, const IR::Value& stream) {
    if (!stream.IsImmediate()) {
        // LOG_WARNING(..., "EndPrimitive's stream is not constant");
        ctx.OpEndStreamPrimitive(ctx.u32_zero_value);
        return;
    }
    ctx.OpEndStreamPrimitive(ctx.Def(stream));
}

} // namespace Shader::Backend::SPIRV
