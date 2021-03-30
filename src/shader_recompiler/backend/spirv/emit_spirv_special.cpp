// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {

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
    }
}

void EmitEpilogue(EmitContext& ctx) {
    if (ctx.profile.convert_depth_mode) {
        const Id type{ctx.F32[1]};
        const Id position{ctx.OpLoad(ctx.F32[4], ctx.output_position)};
        const Id z{ctx.OpCompositeExtract(type, position, 2u)};
        const Id w{ctx.OpCompositeExtract(type, position, 3u)};
        const Id screen_depth{ctx.OpFMul(type, ctx.OpFAdd(type, z, w), ctx.Constant(type, 0.5f))};
        const Id vector{ctx.OpCompositeInsert(ctx.F32[4], screen_depth, position, 2u)};
        ctx.OpStore(ctx.output_position, vector);
    }
}

} // namespace Shader::Backend::SPIRV
