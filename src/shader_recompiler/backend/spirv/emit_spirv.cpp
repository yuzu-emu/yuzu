// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/microinstruction.h"
#include "shader_recompiler/frontend/ir/program.h"

namespace Shader::Backend::SPIRV {
namespace {
template <class Func>
struct FuncTraits : FuncTraits<Func> {};

template <class ReturnType_, class... Args>
struct FuncTraits<ReturnType_ (*)(Args...)> {
    using ReturnType = ReturnType_;

    static constexpr size_t NUM_ARGS = sizeof...(Args);

    template <size_t I>
    using ArgType = std::tuple_element_t<I, std::tuple<Args...>>;
};

template <auto func, typename... Args>
void SetDefinition(EmitContext& ctx, IR::Inst* inst, Args... args) {
    const Id forward_id{inst->Definition<Id>()};
    const bool has_forward_id{Sirit::ValidId(forward_id)};
    Id current_id{};
    if (has_forward_id) {
        current_id = ctx.ExchangeCurrentId(forward_id);
    }
    const Id new_id{func(ctx, std::forward<Args>(args)...)};
    if (has_forward_id) {
        ctx.ExchangeCurrentId(current_id);
    } else {
        inst->SetDefinition<Id>(new_id);
    }
}

template <typename ArgType>
ArgType Arg(EmitContext& ctx, const IR::Value& arg) {
    if constexpr (std::is_same_v<ArgType, Id>) {
        return ctx.Def(arg);
    } else if constexpr (std::is_same_v<ArgType, const IR::Value&>) {
        return arg;
    } else if constexpr (std::is_same_v<ArgType, u32>) {
        return arg.U32();
    } else if constexpr (std::is_same_v<ArgType, IR::Block*>) {
        return arg.Label();
    } else if constexpr (std::is_same_v<ArgType, IR::Attribute>) {
        return arg.Attribute();
    } else if constexpr (std::is_same_v<ArgType, IR::Reg>) {
        return arg.Reg();
    }
}

template <auto func, bool is_first_arg_inst, size_t... I>
void Invoke(EmitContext& ctx, IR::Inst* inst, std::index_sequence<I...>) {
    using Traits = FuncTraits<decltype(func)>;
    if constexpr (std::is_same_v<Traits::ReturnType, Id>) {
        if constexpr (is_first_arg_inst) {
            SetDefinition<func>(ctx, inst, inst, Arg<Traits::ArgType<I + 2>>(ctx, inst->Arg(I))...);
        } else {
            SetDefinition<func>(ctx, inst, Arg<Traits::ArgType<I + 1>>(ctx, inst->Arg(I))...);
        }
    } else {
        if constexpr (is_first_arg_inst) {
            func(ctx, inst, Arg<Traits::ArgType<I + 2>>(ctx, inst->Arg(I))...);
        } else {
            func(ctx, Arg<Traits::ArgType<I + 1>>(ctx, inst->Arg(I))...);
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
    switch (inst->Opcode()) {
#define OPCODE(name, result_type, ...)                                                             \
    case IR::Opcode::name:                                                                         \
        return Invoke<&Emit##name>(ctx, inst);
#include "shader_recompiler/frontend/ir/opcodes.inc"
#undef OPCODE
    }
    throw LogicError("Invalid opcode {}", inst->Opcode());
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

Id DefineMain(EmitContext& ctx, IR::Program& program) {
    const Id void_function{ctx.TypeFunction(ctx.void_id)};
    const Id main{ctx.OpFunction(ctx.void_id, spv::FunctionControlMask::MaskNone, void_function)};
    for (IR::Block* const block : program.blocks) {
        ctx.AddLabel(block->Definition<Id>());
        for (IR::Inst& inst : block->Instructions()) {
            EmitInst(ctx, &inst);
        }
    }
    ctx.OpFunctionEnd();
    return main;
}

void DefineEntryPoint(const IR::Program& program, EmitContext& ctx, Id main) {
    const std::span interfaces(ctx.interfaces.data(), ctx.interfaces.size());
    spv::ExecutionModel execution_model{};
    switch (program.stage) {
    case Shader::Stage::Compute: {
        const std::array<u32, 3> workgroup_size{program.workgroup_size};
        execution_model = spv::ExecutionModel::GLCompute;
        ctx.AddExecutionMode(main, spv::ExecutionMode::LocalSize, workgroup_size[0],
                             workgroup_size[1], workgroup_size[2]);
        break;
    }
    case Shader::Stage::VertexB:
        execution_model = spv::ExecutionModel::Vertex;
        break;
    case Shader::Stage::Fragment:
        execution_model = spv::ExecutionModel::Fragment;
        ctx.AddExecutionMode(main, spv::ExecutionMode::OriginUpperLeft);
        if (program.info.stores_frag_depth) {
            ctx.AddExecutionMode(main, spv::ExecutionMode::DepthReplacing);
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
        // LOG_ERROR(HW_GPU, "Fp32 denorm flush and preserve on the same shader");
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
            // LOG_WARNING(HW_GPU, "Fp32 denorm preserve used in shader without host support");
        }
    }
    if (!profile.support_separate_denorm_behavior) {
        // No separate denorm behavior
        return;
    }
    if (info.uses_fp16_denorms_flush && info.uses_fp16_denorms_preserve) {
        // LOG_ERROR(HW_GPU, "Fp16 denorm flush and preserve on the same shader");
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
            // LOG_WARNING(HW_GPU, "Fp16 denorm preserve used in shader without host support");
        }
    }
}

void SetupSignedNanCapabilities(const Profile& profile, const IR::Program& program,
                                EmitContext& ctx, Id main_func) {
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
    if (info.uses_demote_to_helper_invocation) {
        ctx.AddExtension("SPV_EXT_demote_to_helper_invocation");
        ctx.AddCapability(spv::Capability::DemoteToHelperInvocationEXT);
    }
    if (info.stores_viewport_index) {
        ctx.AddCapability(spv::Capability::MultiViewport);
        if (profile.support_viewport_index_layer_non_geometry &&
            ctx.stage == Shader::Stage::VertexB) {
            ctx.AddExtension("SPV_EXT_shader_viewport_index_layer");
            ctx.AddCapability(spv::Capability::ShaderViewportIndexLayerEXT);
        } else {
            ctx.ignore_viewport_layer = true;
        }
    }
    if (!profile.support_vertex_instance_id && (info.loads_instance_id || info.loads_vertex_id)) {
        ctx.AddExtension("SPV_KHR_shader_draw_parameters");
        ctx.AddCapability(spv::Capability::DrawParameters);
    }
    if ((info.uses_subgroup_vote || info.uses_subgroup_invocation_id) && profile.support_vote) {
        ctx.AddExtension("SPV_KHR_shader_ballot");
        ctx.AddCapability(spv::Capability::SubgroupBallotKHR);
        if (!profile.warp_size_potentially_larger_than_guest) {
            // vote ops are only used when not taking the long path
            ctx.AddExtension("SPV_KHR_subgroup_vote");
            ctx.AddCapability(spv::Capability::SubgroupVoteKHR);
        }
    }
    // TODO: Track this usage
    ctx.AddCapability(spv::Capability::ImageGatherExtended);
    ctx.AddCapability(spv::Capability::ImageQuery);
}

Id PhiArgDef(EmitContext& ctx, IR::Inst* inst, size_t index) {
    // Phi nodes can have forward declarations, if an argument is not defined provide a forward
    // declaration of it. Invoke will take care of giving it the right definition when it's
    // actually defined.
    const IR::Value arg{inst->Arg(index)};
    if (arg.IsImmediate()) {
        // Let the context handle immediate definitions, as it already knows how
        return ctx.Def(arg);
    }
    IR::Inst* const arg_inst{arg.InstRecursive()};
    if (const Id def{arg_inst->Definition<Id>()}; Sirit::ValidId(def)) {
        // Return the current definition if it exists
        return def;
    }
    if (arg_inst == inst) {
        // This is a self referencing phi node
        // Self-referencing definition will be set by the caller, so just grab the current id
        return ctx.CurrentId();
    }
    // If it hasn't been defined and it's not a self reference, get a forward declaration
    const Id def{ctx.ForwardDeclarationId()};
    arg_inst->SetDefinition<Id>(def);
    return def;
}
} // Anonymous namespace

std::vector<u32> EmitSPIRV(const Profile& profile, IR::Program& program, u32& binding) {
    EmitContext ctx{profile, program, binding};
    const Id main{DefineMain(ctx, program)};
    DefineEntryPoint(program, ctx, main);
    if (profile.support_float_controls) {
        ctx.AddExtension("SPV_KHR_float_controls");
        SetupDenormControl(profile, program, ctx, main);
        SetupSignedNanCapabilities(profile, program, ctx, main);
    }
    SetupCapabilities(profile, program.info, ctx);
    return ctx.Assemble();
}

Id EmitPhi(EmitContext& ctx, IR::Inst* inst) {
    const size_t num_args{inst->NumArgs()};
    boost::container::small_vector<Id, 32> operands;
    operands.reserve(num_args * 2);
    for (size_t index = 0; index < num_args; ++index) {
        operands.push_back(PhiArgDef(ctx, inst, index));
        operands.push_back(inst->PhiBlock(index)->Definition<Id>());
    }
    // The type of a phi instruction is stored in its flags
    const Id result_type{TypeId(ctx, inst->Flags<IR::Type>())};
    return ctx.OpPhi(result_type, std::span(operands.data(), operands.size()));
}

void EmitVoid(EmitContext&) {}

Id EmitIdentity(EmitContext& ctx, const IR::Value& value) {
    if (const Id id = ctx.Def(value); Sirit::ValidId(id)) {
        return id;
    }
    const Id def{ctx.ForwardDeclarationId()};
    value.InstRecursive()->SetDefinition<Id>(def);
    return def;
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
