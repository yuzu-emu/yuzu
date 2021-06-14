// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"

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

void SetFixedPipelinePointSize(EmitContext& ctx) {
    if (ctx.runtime_info.fixed_state_point_size) {
        const float point_size{*ctx.runtime_info.fixed_state_point_size};
        ctx.OpStore(ctx.output_point_size, ctx.Const(point_size));
    }
}

Id DefaultVarying(EmitContext& ctx, u32 num_components, u32 element, Id zero, Id one,
                  Id default_vector) {
    switch (num_components) {
    case 1:
        return element == 3 ? one : zero;
    case 2:
        return ctx.ConstantComposite(ctx.F32[2], zero, element + 1 == 3 ? one : zero);
    case 3:
        return ctx.ConstantComposite(ctx.F32[3], zero, zero, element + 2 == 3 ? one : zero);
    case 4:
        return default_vector;
    }
    throw InvalidArgument("Bad element");
}

Id ComparisonFunction(EmitContext& ctx, CompareFunction comparison, Id operand_1, Id operand_2) {
    switch (comparison) {
    case CompareFunction::Never:
        return ctx.false_value;
    case CompareFunction::Less:
        return ctx.OpFOrdLessThan(ctx.U1, operand_1, operand_2);
    case CompareFunction::Equal:
        return ctx.OpFOrdEqual(ctx.U1, operand_1, operand_2);
    case CompareFunction::LessThanEqual:
        return ctx.OpFOrdLessThanEqual(ctx.U1, operand_1, operand_2);
    case CompareFunction::Greater:
        return ctx.OpFOrdGreaterThan(ctx.U1, operand_1, operand_2);
    case CompareFunction::NotEqual:
        return ctx.OpFOrdNotEqual(ctx.U1, operand_1, operand_2);
    case CompareFunction::GreaterThanEqual:
        return ctx.OpFOrdGreaterThanEqual(ctx.U1, operand_1, operand_2);
    case CompareFunction::Always:
        return ctx.true_value;
    }
    throw InvalidArgument("Comparison function {}", comparison);
}

void AlphaTest(EmitContext& ctx) {
    if (!ctx.runtime_info.alpha_test_func) {
        return;
    }
    const auto comparison{*ctx.runtime_info.alpha_test_func};
    if (comparison == CompareFunction::Always) {
        return;
    }
    if (!Sirit::ValidId(ctx.frag_color[0])) {
        return;
    }

    const Id type{ctx.F32[1]};
    const Id rt0_color{ctx.OpLoad(ctx.F32[4], ctx.frag_color[0])};
    const Id alpha{ctx.OpCompositeExtract(type, rt0_color, 3u)};

    const Id true_label{ctx.OpLabel()};
    const Id discard_label{ctx.OpLabel()};
    const Id alpha_reference{ctx.Const(ctx.runtime_info.alpha_test_reference)};
    const Id condition{ComparisonFunction(ctx, comparison, alpha, alpha_reference)};

    ctx.OpSelectionMerge(true_label, spv::SelectionControlMask::MaskNone);
    ctx.OpBranchConditional(condition, true_label, discard_label);
    ctx.AddLabel(discard_label);
    ctx.OpKill();
    ctx.AddLabel(true_label);
}
} // Anonymous namespace

void EmitPrologue(EmitContext& ctx) {
    if (ctx.stage == Stage::VertexB) {
        const Id zero{ctx.Const(0.0f)};
        const Id one{ctx.Const(1.0f)};
        const Id default_vector{ctx.ConstantComposite(ctx.F32[4], zero, zero, zero, one)};
        ctx.OpStore(ctx.output_position, default_vector);
        for (const auto& info : ctx.output_generics) {
            if (info[0].num_components == 0) {
                continue;
            }
            u32 element{0};
            while (element < 4) {
                const auto& element_info{info[element]};
                const u32 num{element_info.num_components};
                const Id value{DefaultVarying(ctx, num, element, zero, one, default_vector)};
                ctx.OpStore(element_info.id, value);
                element += num;
            }
        }
    }
    if (ctx.stage == Stage::VertexB || ctx.stage == Stage::Geometry) {
        SetFixedPipelinePointSize(ctx);
    }
}

void EmitEpilogue(EmitContext& ctx) {
    if (ctx.stage == Stage::VertexB && ctx.runtime_info.convert_depth_mode) {
        ConvertDepthMode(ctx);
    }
    if (ctx.stage == Stage::Fragment) {
        AlphaTest(ctx);
    }
}

void EmitEmitVertex(EmitContext& ctx, const IR::Value& stream) {
    if (ctx.runtime_info.convert_depth_mode) {
        ConvertDepthMode(ctx);
    }
    if (stream.IsImmediate()) {
        ctx.OpEmitStreamVertex(ctx.Def(stream));
    } else {
        LOG_WARNING(Shader_SPIRV, "Stream is not immediate");
        ctx.OpEmitStreamVertex(ctx.u32_zero_value);
    }
    // Restore fixed pipeline point size after emitting the vertex
    SetFixedPipelinePointSize(ctx);
}

void EmitEndPrimitive(EmitContext& ctx, const IR::Value& stream) {
    if (stream.IsImmediate()) {
        ctx.OpEndStreamPrimitive(ctx.Def(stream));
    } else {
        LOG_WARNING(Shader_SPIRV, "Stream is not immediate");
        ctx.OpEndStreamPrimitive(ctx.u32_zero_value);
    }
}

} // namespace Shader::Backend::SPIRV
