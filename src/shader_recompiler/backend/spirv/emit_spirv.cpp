// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "common/settings.h"
#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/program.h"

namespace Shader::Backend::SPIRV {
namespace {
template <class Func>
struct FuncTraits {};

template <class ReturnType_, class... Args>
struct FuncTraits<ReturnType_ (*)(Args...)> {
    using ReturnType = ReturnType_;

    static constexpr size_t NUM_ARGS = sizeof...(Args);

    template <size_t I>
    using ArgType = std::tuple_element_t<I, std::tuple<Args...>>;
};

template <auto func, typename... Args>
void SetDefinition(EmitContext& ctx, IR::Inst* inst, Args... args) {
    inst->SetDefinition<Id>(func(ctx, std::forward<Args>(args)...));
}

template <typename ArgType>
ArgType Arg(EmitContext& ctx, const IR::Value& arg) {
    if constexpr (std::is_same_v<ArgType, Id>) {
        return ctx.Def(arg);
    } else if constexpr (std::is_same_v<ArgType, const IR::Value&>) {
        return arg;
    } else if constexpr (std::is_same_v<ArgType, u32>) {
        return arg.U32();
    } else if constexpr (std::is_same_v<ArgType, IR::Attribute>) {
        return arg.Attribute();
    } else if constexpr (std::is_same_v<ArgType, IR::Patch>) {
        return arg.Patch();
    } else if constexpr (std::is_same_v<ArgType, IR::Reg>) {
        return arg.Reg();
    }
}

template <auto func, bool is_first_arg_inst, size_t... I>
void Invoke(EmitContext& ctx, IR::Inst* inst, std::index_sequence<I...>) {
    using Traits = FuncTraits<decltype(func)>;
    if constexpr (std::is_same_v<typename Traits::ReturnType, Id>) {
        if constexpr (is_first_arg_inst) {
            SetDefinition<func>(
                ctx, inst, inst,
                Arg<typename Traits::template ArgType<I + 2>>(ctx, inst->Arg(I))...);
        } else {
            SetDefinition<func>(
                ctx, inst, Arg<typename Traits::template ArgType<I + 1>>(ctx, inst->Arg(I))...);
        }
    } else {
        if constexpr (is_first_arg_inst) {
            func(ctx, inst, Arg<typename Traits::template ArgType<I + 2>>(ctx, inst->Arg(I))...);
        } else {
            func(ctx, Arg<typename Traits::template ArgType<I + 1>>(ctx, inst->Arg(I))...);
        }
    }
}

template <auto func>
void Invoke(EmitContext& ctx, IR::Inst* inst) {
    using Traits = FuncTraits<decltype(func)>;
    static_assert(Traits::NUM_ARGS >= 1, "Insufficient arguments");
    if constexpr (Traits::NUM_ARGS == 1) {
        Invoke<func, false>(ctx, inst, std::make_index_sequence<0>{});
    } else {
        using FirstArgType = typename Traits::template ArgType<1>;
        static constexpr bool is_first_arg_inst = std::is_same_v<FirstArgType, IR::Inst*>;
        using Indices = std::make_index_sequence<Traits::NUM_ARGS - (is_first_arg_inst ? 2 : 1)>;
        Invoke<func, is_first_arg_inst>(ctx, inst, Indices{});
    }
}

void EmitInst(EmitContext& ctx, IR::Inst* inst) {
    switch (inst->GetOpcode()) {
#define OPCODE(name, result_type, ...)                                                             \
    case IR::Opcode::name:                                                                         \
        return Invoke<&Emit##name>(ctx, inst);
#include "shader_recompiler/frontend/ir/opcodes.inc"
#undef OPCODE
    }
    throw LogicError("Invalid opcode {}", inst->GetOpcode());
}

Id TypeId(const EmitContext& ctx, IR::Type type) {
    switch (type) {
    case IR::Type::U1:
        return ctx.U1;
    case IR::Type::U32:
        return ctx.U32[1];
    default:
        throw NotImplementedException("Phi node type {}", type);
    }
}

void Traverse(EmitContext& ctx, IR::Program& program) {
    IR::Block* current_block{};
    for (const IR::AbstractSyntaxNode& node : program.syntax_list) {
        switch (node.type) {
        case IR::AbstractSyntaxNode::Type::Block: {
            const Id label{node.data.block->Definition<Id>()};
            if (current_block) {
                ctx.OpBranch(label);
            }
            current_block = node.data.block;
            ctx.AddLabel(label);
            for (IR::Inst& inst : node.data.block->Instructions()) {
                EmitInst(ctx, &inst);
            }
            break;
        }
        case IR::AbstractSyntaxNode::Type::If: {
            const Id if_label{node.data.if_node.body->Definition<Id>()};
            const Id endif_label{node.data.if_node.merge->Definition<Id>()};
            ctx.OpSelectionMerge(endif_label, spv::SelectionControlMask::MaskNone);
            ctx.OpBranchConditional(ctx.Def(node.data.if_node.cond), if_label, endif_label);
            break;
        }
        case IR::AbstractSyntaxNode::Type::Loop: {
            const Id body_label{node.data.loop.body->Definition<Id>()};
            const Id continue_label{node.data.loop.continue_block->Definition<Id>()};
            const Id endloop_label{node.data.loop.merge->Definition<Id>()};

            ctx.OpLoopMerge(endloop_label, continue_label, spv::LoopControlMask::MaskNone);
            ctx.OpBranch(body_label);
            break;
        }
        case IR::AbstractSyntaxNode::Type::Break: {
            const Id break_label{node.data.break_node.merge->Definition<Id>()};
            const Id skip_label{node.data.break_node.skip->Definition<Id>()};
            ctx.OpBranchConditional(ctx.Def(node.data.break_node.cond), break_label, skip_label);
            break;
        }
        case IR::AbstractSyntaxNode::Type::EndIf:
            if (current_block) {
                ctx.OpBranch(node.data.end_if.merge->Definition<Id>());
            }
            break;
        case IR::AbstractSyntaxNode::Type::Repeat: {
            Id cond{ctx.Def(node.data.repeat.cond)};
            if (!Settings::values.disable_shader_loop_safety_checks) {
                const Id pointer_type{ctx.TypePointer(spv::StorageClass::Private, ctx.U32[1])};
                const Id safety_counter{ctx.AddGlobalVariable(
                    pointer_type, spv::StorageClass::Private, ctx.Const(0x2000u))};
                if (ctx.profile.supported_spirv >= 0x00010400) {
                    ctx.interfaces.push_back(safety_counter);
                }
                const Id old_counter{ctx.OpLoad(ctx.U32[1], safety_counter)};
                const Id new_counter{ctx.OpISub(ctx.U32[1], old_counter, ctx.Const(1u))};
                ctx.OpStore(safety_counter, new_counter);

                const Id safety_cond{
                    ctx.OpSGreaterThanEqual(ctx.U1, new_counter, ctx.u32_zero_value)};
                cond = ctx.OpLogicalAnd(ctx.U1, cond, safety_cond);
            }
            const Id loop_header_label{node.data.repeat.loop_header->Definition<Id>()};
            const Id merge_label{node.data.repeat.merge->Definition<Id>()};
            ctx.OpBranchConditional(cond, loop_header_label, merge_label);
            break;
        }
        case IR::AbstractSyntaxNode::Type::Return:
            ctx.OpReturn();
            break;
        case IR::AbstractSyntaxNode::Type::Unreachable:
            ctx.OpUnreachable();
            break;
        }
        if (node.type != IR::AbstractSyntaxNode::Type::Block) {
            current_block = nullptr;
        }
    }
}

Id DefineMain(EmitContext& ctx, IR::Program& program) {
    const Id void_function{ctx.TypeFunction(ctx.void_id)};
    const Id main{ctx.OpFunction(ctx.void_id, spv::FunctionControlMask::MaskNone, void_function)};
    for (IR::Block* const block : program.blocks) {
        block->SetDefinition(ctx.OpLabel());
    }
    Traverse(ctx, program);
    ctx.OpFunctionEnd();
    return main;
}

spv::ExecutionMode ExecutionMode(TessPrimitive primitive) {
    switch (primitive) {
    case TessPrimitive::Isolines:
        return spv::ExecutionMode::Isolines;
    case TessPrimitive::Triangles:
        return spv::ExecutionMode::Triangles;
    case TessPrimitive::Quads:
        return spv::ExecutionMode::Quads;
    }
    throw InvalidArgument("Tessellation primitive {}", primitive);
}

spv::ExecutionMode ExecutionMode(TessSpacing spacing) {
    switch (spacing) {
    case TessSpacing::Equal:
        return spv::ExecutionMode::SpacingEqual;
    case TessSpacing::FractionalOdd:
        return spv::ExecutionMode::SpacingFractionalOdd;
    case TessSpacing::FractionalEven:
        return spv::ExecutionMode::SpacingFractionalEven;
    }
    throw InvalidArgument("Tessellation spacing {}", spacing);
}

void DefineEntryPoint(const IR::Program& program, EmitContext& ctx, Id main) {
    const std::span interfaces(ctx.interfaces.data(), ctx.interfaces.size());
    spv::ExecutionModel execution_model{};
    switch (program.stage) {
    case Stage::Compute: {
        const std::array<u32, 3> workgroup_size{program.workgroup_size};
        execution_model = spv::ExecutionModel::GLCompute;
        ctx.AddExecutionMode(main, spv::ExecutionMode::LocalSize, workgroup_size[0],
                             workgroup_size[1], workgroup_size[2]);
        break;
    }
    case Stage::VertexB:
        execution_model = spv::ExecutionModel::Vertex;
        break;
    case Stage::TessellationControl:
        execution_model = spv::ExecutionModel::TessellationControl;
        ctx.AddCapability(spv::Capability::Tessellation);
        ctx.AddExecutionMode(main, spv::ExecutionMode::OutputVertices, program.invocations);
        break;
    case Stage::TessellationEval:
        execution_model = spv::ExecutionModel::TessellationEvaluation;
        ctx.AddCapability(spv::Capability::Tessellation);
        ctx.AddExecutionMode(main, ExecutionMode(ctx.runtime_info.tess_primitive));
        ctx.AddExecutionMode(main, ExecutionMode(ctx.runtime_info.tess_spacing));
        ctx.AddExecutionMode(main, ctx.runtime_info.tess_clockwise
                                       ? spv::ExecutionMode::VertexOrderCw
                                       : spv::ExecutionMode::VertexOrderCcw);
        break;
    case Stage::Geometry:
        execution_model = spv::ExecutionModel::Geometry;
        ctx.AddCapability(spv::Capability::Geometry);
        ctx.AddCapability(spv::Capability::GeometryStreams);
        switch (ctx.runtime_info.input_topology) {
        case InputTopology::Points:
            ctx.AddExecutionMode(main, spv::ExecutionMode::InputPoints);
            break;
        case InputTopology::Lines:
            ctx.AddExecutionMode(main, spv::ExecutionMode::InputLines);
            break;
        case InputTopology::LinesAdjacency:
            ctx.AddExecutionMode(main, spv::ExecutionMode::InputLinesAdjacency);
            break;
        case InputTopology::Triangles:
            ctx.AddExecutionMode(main, spv::ExecutionMode::Triangles);
            break;
        case InputTopology::TrianglesAdjacency:
            ctx.AddExecutionMode(main, spv::ExecutionMode::InputTrianglesAdjacency);
            break;
        }
        switch (program.output_topology) {
        case OutputTopology::PointList:
            ctx.AddExecutionMode(main, spv::ExecutionMode::OutputPoints);
            break;
        case OutputTopology::LineStrip:
            ctx.AddExecutionMode(main, spv::ExecutionMode::OutputLineStrip);
            break;
        case OutputTopology::TriangleStrip:
            ctx.AddExecutionMode(main, spv::ExecutionMode::OutputTriangleStrip);
            break;
        }
        if (program.info.stores[IR::Attribute::PointSize]) {
            ctx.AddCapability(spv::Capability::GeometryPointSize);
        }
        ctx.AddExecutionMode(main, spv::ExecutionMode::OutputVertices, program.output_vertices);
        ctx.AddExecutionMode(main, spv::ExecutionMode::Invocations, program.invocations);
        if (program.is_geometry_passthrough) {
            if (ctx.profile.support_geometry_shader_passthrough) {
                ctx.AddExtension("SPV_NV_geometry_shader_passthrough");
                ctx.AddCapability(spv::Capability::GeometryShaderPassthroughNV);
            } else {
                LOG_WARNING(Shader_SPIRV, "Geometry shader passthrough used with no support");
            }
        }
        break;
    case Stage::Fragment:
        execution_model = spv::ExecutionModel::Fragment;
        if (ctx.profile.lower_left_origin_mode) {
            ctx.AddExecutionMode(main, spv::ExecutionMode::OriginLowerLeft);
        } else {
            ctx.AddExecutionMode(main, spv::ExecutionMode::OriginUpperLeft);
        }
        if (program.info.stores_frag_depth) {
            ctx.AddExecutionMode(main, spv::ExecutionMode::DepthReplacing);
        }
        if (ctx.runtime_info.force_early_z) {
            ctx.AddExecutionMode(main, spv::ExecutionMode::EarlyFragmentTests);
        }
        break;
    default:
        throw NotImplementedException("Stage {}", program.stage);
    }
    ctx.AddEntryPoint(execution_model, main, "main", interfaces);
}

void SetupDenormControl(const Profile& profile, const IR::Program& program, EmitContext& ctx,
                        Id main_func) {
    const Info& info{program.info};
    if (info.uses_fp32_denorms_flush && info.uses_fp32_denorms_preserve) {
        LOG_DEBUG(Shader_SPIRV, "Fp32 denorm flush and preserve on the same shader");
    } else if (info.uses_fp32_denorms_flush) {
        if (profile.support_fp32_denorm_flush) {
            ctx.AddCapability(spv::Capability::DenormFlushToZero);
            ctx.AddExecutionMode(main_func, spv::ExecutionMode::DenormFlushToZero, 32U);
        } else {
            // Drivers will most likely flush denorms by default, no need to warn
        }
    } else if (info.uses_fp32_denorms_preserve) {
        if (profile.support_fp32_denorm_preserve) {
            ctx.AddCapability(spv::Capability::DenormPreserve);
            ctx.AddExecutionMode(main_func, spv::ExecutionMode::DenormPreserve, 32U);
        } else {
            LOG_DEBUG(Shader_SPIRV, "Fp32 denorm preserve used in shader without host support");
        }
    }
    if (!profile.support_separate_denorm_behavior || profile.has_broken_fp16_float_controls) {
        // No separate denorm behavior
        return;
    }
    if (info.uses_fp16_denorms_flush && info.uses_fp16_denorms_preserve) {
        LOG_DEBUG(Shader_SPIRV, "Fp16 denorm flush and preserve on the same shader");
    } else if (info.uses_fp16_denorms_flush) {
        if (profile.support_fp16_denorm_flush) {
            ctx.AddCapability(spv::Capability::DenormFlushToZero);
            ctx.AddExecutionMode(main_func, spv::ExecutionMode::DenormFlushToZero, 16U);
        } else {
            // Same as fp32, no need to warn as most drivers will flush by default
        }
    } else if (info.uses_fp16_denorms_preserve) {
        if (profile.support_fp16_denorm_preserve) {
            ctx.AddCapability(spv::Capability::DenormPreserve);
            ctx.AddExecutionMode(main_func, spv::ExecutionMode::DenormPreserve, 16U);
        } else {
            LOG_DEBUG(Shader_SPIRV, "Fp16 denorm preserve used in shader without host support");
        }
    }
}

void SetupSignedNanCapabilities(const Profile& profile, const IR::Program& program,
                                EmitContext& ctx, Id main_func) {
    if (profile.has_broken_fp16_float_controls && program.info.uses_fp16) {
        return;
    }
    if (program.info.uses_fp16 && profile.support_fp16_signed_zero_nan_preserve) {
        ctx.AddCapability(spv::Capability::SignedZeroInfNanPreserve);
        ctx.AddExecutionMode(main_func, spv::ExecutionMode::SignedZeroInfNanPreserve, 16U);
    }
    if (profile.support_fp32_signed_zero_nan_preserve) {
        ctx.AddCapability(spv::Capability::SignedZeroInfNanPreserve);
        ctx.AddExecutionMode(main_func, spv::ExecutionMode::SignedZeroInfNanPreserve, 32U);
    }
    if (program.info.uses_fp64 && profile.support_fp64_signed_zero_nan_preserve) {
        ctx.AddCapability(spv::Capability::SignedZeroInfNanPreserve);
        ctx.AddExecutionMode(main_func, spv::ExecutionMode::SignedZeroInfNanPreserve, 64U);
    }
}

void SetupCapabilities(const Profile& profile, const Info& info, EmitContext& ctx) {
    if (info.uses_sampled_1d) {
        ctx.AddCapability(spv::Capability::Sampled1D);
    }
    if (info.uses_sparse_residency) {
        ctx.AddCapability(spv::Capability::SparseResidency);
    }
    if (info.uses_demote_to_helper_invocation && profile.support_demote_to_helper_invocation) {
        ctx.AddExtension("SPV_EXT_demote_to_helper_invocation");
        ctx.AddCapability(spv::Capability::DemoteToHelperInvocationEXT);
    }
    if (info.stores[IR::Attribute::ViewportIndex]) {
        ctx.AddCapability(spv::Capability::MultiViewport);
    }
    if (info.stores[IR::Attribute::ViewportMask] && profile.support_viewport_mask) {
        ctx.AddExtension("SPV_NV_viewport_array2");
        ctx.AddCapability(spv::Capability::ShaderViewportMaskNV);
    }
    if (info.stores[IR::Attribute::Layer] || info.stores[IR::Attribute::ViewportIndex]) {
        if (profile.support_viewport_index_layer_non_geometry && ctx.stage != Stage::Geometry) {
            ctx.AddExtension("SPV_EXT_shader_viewport_index_layer");
            ctx.AddCapability(spv::Capability::ShaderViewportIndexLayerEXT);
        }
    }
    if (!profile.support_vertex_instance_id &&
        (info.loads[IR::Attribute::InstanceId] || info.loads[IR::Attribute::VertexId])) {
        ctx.AddExtension("SPV_KHR_shader_draw_parameters");
        ctx.AddCapability(spv::Capability::DrawParameters);
    }
    if ((info.uses_subgroup_vote || info.uses_subgroup_invocation_id ||
         info.uses_subgroup_shuffles) &&
        profile.support_vote) {
        ctx.AddExtension("SPV_KHR_shader_ballot");
        ctx.AddCapability(spv::Capability::SubgroupBallotKHR);
        if (!profile.warp_size_potentially_larger_than_guest) {
            // vote ops are only used when not taking the long path
            ctx.AddExtension("SPV_KHR_subgroup_vote");
            ctx.AddCapability(spv::Capability::SubgroupVoteKHR);
        }
    }
    if (info.uses_int64_bit_atomics && profile.support_int64_atomics) {
        ctx.AddCapability(spv::Capability::Int64Atomics);
    }
    if (info.uses_typeless_image_reads && profile.support_typeless_image_loads) {
        ctx.AddCapability(spv::Capability::StorageImageReadWithoutFormat);
    }
    if (info.uses_typeless_image_writes) {
        ctx.AddCapability(spv::Capability::StorageImageWriteWithoutFormat);
    }
    if (info.uses_image_buffers) {
        ctx.AddCapability(spv::Capability::ImageBuffer);
    }
    if (info.uses_sample_id) {
        ctx.AddCapability(spv::Capability::SampleRateShading);
    }
    if (!ctx.runtime_info.xfb_varyings.empty()) {
        ctx.AddCapability(spv::Capability::TransformFeedback);
    }
    if (info.uses_derivatives) {
        ctx.AddCapability(spv::Capability::DerivativeControl);
    }
    // TODO: Track this usage
    ctx.AddCapability(spv::Capability::ImageGatherExtended);
    ctx.AddCapability(spv::Capability::ImageQuery);
    ctx.AddCapability(spv::Capability::SampledBuffer);
}

void PatchPhiNodes(IR::Program& program, EmitContext& ctx) {
    auto inst{program.blocks.front()->begin()};
    size_t block_index{0};
    ctx.PatchDeferredPhi([&](size_t phi_arg) {
        if (phi_arg == 0) {
            ++inst;
            if (inst == program.blocks[block_index]->end() ||
                inst->GetOpcode() != IR::Opcode::Phi) {
                do {
                    ++block_index;
                    inst = program.blocks[block_index]->begin();
                } while (inst->GetOpcode() != IR::Opcode::Phi);
            }
        }
        return ctx.Def(inst->Arg(phi_arg));
    });
}
} // Anonymous namespace

std::vector<u32> EmitSPIRV(const Profile& profile, const RuntimeInfo& runtime_info,
                           IR::Program& program, Bindings& bindings) {
    EmitContext ctx{profile, runtime_info, program, bindings};
    const Id main{DefineMain(ctx, program)};
    DefineEntryPoint(program, ctx, main);
    if (profile.support_float_controls) {
        ctx.AddExtension("SPV_KHR_float_controls");
        SetupDenormControl(profile, program, ctx, main);
        SetupSignedNanCapabilities(profile, program, ctx, main);
    }
    SetupCapabilities(profile, program.info, ctx);
    PatchPhiNodes(program, ctx);
    return ctx.Assemble();
}

Id EmitPhi(EmitContext& ctx, IR::Inst* inst) {
    const size_t num_args{inst->NumArgs()};
    boost::container::small_vector<Id, 32> blocks;
    blocks.reserve(num_args);
    for (size_t index = 0; index < num_args; ++index) {
        blocks.push_back(inst->PhiBlock(index)->Definition<Id>());
    }
    // The type of a phi instruction is stored in its flags
    const Id result_type{TypeId(ctx, inst->Flags<IR::Type>())};
    return ctx.DeferredOpPhi(result_type, std::span(blocks.data(), blocks.size()));
}

void EmitVoid(EmitContext&) {}

Id EmitIdentity(EmitContext& ctx, const IR::Value& value) {
    const Id id{ctx.Def(value)};
    if (!Sirit::ValidId(id)) {
        throw NotImplementedException("Forward identity declaration");
    }
    return id;
}

Id EmitConditionRef(EmitContext& ctx, const IR::Value& value) {
    const Id id{ctx.Def(value)};
    if (!Sirit::ValidId(id)) {
        throw NotImplementedException("Forward identity declaration");
    }
    return id;
}

void EmitReference(EmitContext&) {}

void EmitPhiMove(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitGetZeroFromOp(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitGetSignFromOp(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitGetCarryFromOp(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitGetOverflowFromOp(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitGetSparseFromOp(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitGetInBoundsFromOp(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

} // namespace Shader::Backend::SPIRV
