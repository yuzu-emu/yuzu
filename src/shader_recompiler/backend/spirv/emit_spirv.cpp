// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <numeric>
#include <type_traits>

#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/function.h"
#include "shader_recompiler/frontend/ir/microinstruction.h"
#include "shader_recompiler/frontend/ir/program.h"

namespace Shader::Backend::SPIRV {
namespace {
template <class Func>
struct FuncTraits : FuncTraits<decltype(&Func::operator())> {};

template <class ClassType, class ReturnType_, class... Args>
struct FuncTraits<ReturnType_ (ClassType::*)(Args...)> {
    using ReturnType = ReturnType_;

    static constexpr size_t NUM_ARGS = sizeof...(Args);

    template <size_t I>
    using ArgType = std::tuple_element_t<I, std::tuple<Args...>>;
};

template <auto method, typename... Args>
void SetDefinition(EmitSPIRV& emit, EmitContext& ctx, IR::Inst* inst, Args... args) {
    const Id forward_id{inst->Definition<Id>()};
    const bool has_forward_id{Sirit::ValidId(forward_id)};
    Id current_id{};
    if (has_forward_id) {
        current_id = ctx.ExchangeCurrentId(forward_id);
    }
    const Id new_id{(emit.*method)(ctx, std::forward<Args>(args)...)};
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
    }
}

template <auto method, bool is_first_arg_inst, size_t... I>
void Invoke(EmitSPIRV& emit, EmitContext& ctx, IR::Inst* inst, std::index_sequence<I...>) {
    using Traits = FuncTraits<decltype(method)>;
    if constexpr (std::is_same_v<Traits::ReturnType, Id>) {
        if constexpr (is_first_arg_inst) {
            SetDefinition<method>(emit, ctx, inst, inst,
                                  Arg<Traits::ArgType<I + 2>>(ctx, inst->Arg(I))...);
        } else {
            SetDefinition<method>(emit, ctx, inst,
                                  Arg<Traits::ArgType<I + 1>>(ctx, inst->Arg(I))...);
        }
    } else {
        if constexpr (is_first_arg_inst) {
            (emit.*method)(ctx, inst, Arg<Traits::ArgType<I + 2>>(ctx, inst->Arg(I))...);
        } else {
            (emit.*method)(ctx, Arg<Traits::ArgType<I + 1>>(ctx, inst->Arg(I))...);
        }
    }
}

template <auto method>
void Invoke(EmitSPIRV& emit, EmitContext& ctx, IR::Inst* inst) {
    using Traits = FuncTraits<decltype(method)>;
    static_assert(Traits::NUM_ARGS >= 1, "Insufficient arguments");
    if constexpr (Traits::NUM_ARGS == 1) {
        Invoke<method, false>(emit, ctx, inst, std::make_index_sequence<0>{});
    } else {
        using FirstArgType = typename Traits::template ArgType<1>;
        static constexpr bool is_first_arg_inst = std::is_same_v<FirstArgType, IR::Inst*>;
        using Indices = std::make_index_sequence<Traits::NUM_ARGS - (is_first_arg_inst ? 2 : 1)>;
        Invoke<method, is_first_arg_inst>(emit, ctx, inst, Indices{});
    }
}
} // Anonymous namespace

EmitSPIRV::EmitSPIRV(IR::Program& program) {
    EmitContext ctx{program};
    const Id void_function{ctx.TypeFunction(ctx.void_id)};
    // FIXME: Forward declare functions (needs sirit support)
    Id func{};
    for (IR::Function& function : program.functions) {
        func = ctx.OpFunction(ctx.void_id, spv::FunctionControlMask::MaskNone, void_function);
        for (IR::Block* const block : function.blocks) {
            ctx.AddLabel(block->Definition<Id>());
            for (IR::Inst& inst : block->Instructions()) {
                EmitInst(ctx, &inst);
            }
        }
        ctx.OpFunctionEnd();
    }
    boost::container::small_vector<Id, 32> interfaces;
    if (program.info.uses_workgroup_id) {
        interfaces.push_back(ctx.workgroup_id);
    }
    if (program.info.uses_local_invocation_id) {
        interfaces.push_back(ctx.local_invocation_id);
    }

    const std::span interfaces_span(interfaces.data(), interfaces.size());
    ctx.AddEntryPoint(spv::ExecutionModel::Fragment, func, "main", interfaces_span);
    ctx.AddExecutionMode(func, spv::ExecutionMode::OriginUpperLeft);

    std::vector<u32> result{ctx.Assemble()};
    std::FILE* file{std::fopen("D:\\shader.spv", "wb")};
    std::fwrite(result.data(), sizeof(u32), result.size(), file);
    std::fclose(file);
    std::system("spirv-dis D:\\shader.spv") == 0 &&
        std::system("spirv-val --uniform-buffer-standard-layout D:\\shader.spv") == 0 &&
        std::system("spirv-cross -V D:\\shader.spv") == 0;
}

void EmitSPIRV::EmitInst(EmitContext& ctx, IR::Inst* inst) {
    switch (inst->Opcode()) {
#define OPCODE(name, result_type, ...)                                                             \
    case IR::Opcode::name:                                                                         \
        return Invoke<&EmitSPIRV::Emit##name>(*this, ctx, inst);
#include "shader_recompiler/frontend/ir/opcodes.inc"
#undef OPCODE
    }
    throw LogicError("Invalid opcode {}", inst->Opcode());
}

static Id TypeId(const EmitContext& ctx, IR::Type type) {
    switch (type) {
    case IR::Type::U1:
        return ctx.U1;
    case IR::Type::U32:
        return ctx.U32[1];
    default:
        throw NotImplementedException("Phi node type {}", type);
    }
}

Id EmitSPIRV::EmitPhi(EmitContext& ctx, IR::Inst* inst) {
    const size_t num_args{inst->NumArgs()};
    boost::container::small_vector<Id, 32> operands;
    operands.reserve(num_args * 2);
    for (size_t index = 0; index < num_args; ++index) {
        // Phi nodes can have forward declarations, if an argument is not defined provide a forward
        // declaration of it. Invoke will take care of giving it the right definition when it's
        // actually defined.
        const IR::Value arg{inst->Arg(index)};
        Id def{};
        if (arg.IsImmediate()) {
            // Let the context handle immediate definitions, as it already knows how
            def = ctx.Def(arg);
        } else {
            IR::Inst* const arg_inst{arg.Inst()};
            def = arg_inst->Definition<Id>();
            if (!Sirit::ValidId(def)) {
                // If it hasn't been defined, get a forward declaration
                def = ctx.ForwardDeclarationId();
                arg_inst->SetDefinition<Id>(def);
            }
        }
        IR::Block* const phi_block{inst->PhiBlock(index)};
        operands.push_back(def);
        operands.push_back(phi_block->Definition<Id>());
    }
    const Id result_type{TypeId(ctx, inst->Arg(0).Type())};
    return ctx.OpPhi(result_type, std::span(operands.data(), operands.size()));
}

void EmitSPIRV::EmitVoid(EmitContext&) {}

Id EmitSPIRV::EmitIdentity(EmitContext& ctx, const IR::Value& value) {
    return ctx.Def(value);
}

void EmitSPIRV::EmitGetZeroFromOp(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitSPIRV::EmitGetSignFromOp(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitSPIRV::EmitGetCarryFromOp(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitSPIRV::EmitGetOverflowFromOp(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

} // namespace Shader::Backend::SPIRV
