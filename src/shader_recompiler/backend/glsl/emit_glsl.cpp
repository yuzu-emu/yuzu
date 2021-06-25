// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <ranges>
#include <string>

#include "common/div_ceil.h"
#include "common/settings.h"
#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"

namespace Shader::Backend::GLSL {
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
auto Arg(EmitContext& ctx, const IR::Value& arg) {
    if constexpr (std::is_same_v<ArgType, std::string_view>) {
        return ctx.var_alloc.Consume(arg);
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
                ctx, inst, *inst,
                Arg<typename Traits::template ArgType<I + 2>>(ctx, inst->Arg(I))...);
        } else {
            SetDefinition<func>(
                ctx, inst, Arg<typename Traits::template ArgType<I + 1>>(ctx, inst->Arg(I))...);
        }
    } else {
        if constexpr (is_first_arg_inst) {
            func(ctx, *inst, Arg<typename Traits::template ArgType<I + 2>>(ctx, inst->Arg(I))...);
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
        static constexpr bool is_first_arg_inst = std::is_same_v<FirstArgType, IR::Inst&>;
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

bool IsReference(IR::Inst& inst) {
    return inst.GetOpcode() == IR::Opcode::Reference;
}

void PrecolorInst(IR::Inst& phi) {
    // Insert phi moves before references to avoid overwritting other phis
    const size_t num_args{phi.NumArgs()};
    for (size_t i = 0; i < num_args; ++i) {
        IR::Block& phi_block{*phi.PhiBlock(i)};
        auto it{std::find_if_not(phi_block.rbegin(), phi_block.rend(), IsReference).base()};
        IR::IREmitter ir{phi_block, it};
        const IR::Value arg{phi.Arg(i)};
        if (arg.IsImmediate()) {
            ir.PhiMove(phi, arg);
        } else {
            ir.PhiMove(phi, IR::Value{arg.InstRecursive()});
        }
    }
    for (size_t i = 0; i < num_args; ++i) {
        IR::IREmitter{*phi.PhiBlock(i)}.Reference(IR::Value{&phi});
    }
}

void Precolor(const IR::Program& program) {
    for (IR::Block* const block : program.blocks) {
        for (IR::Inst& phi : block->Instructions() | std::views::take_while(IR::IsPhi)) {
            PrecolorInst(phi);
        }
    }
}

void EmitCode(EmitContext& ctx, const IR::Program& program) {
    for (const IR::AbstractSyntaxNode& node : program.syntax_list) {
        switch (node.type) {
        case IR::AbstractSyntaxNode::Type::Block:
            for (IR::Inst& inst : node.data.block->Instructions()) {
                EmitInst(ctx, &inst);
            }
            break;
        case IR::AbstractSyntaxNode::Type::If:
            ctx.Add("if({}){{", ctx.var_alloc.Consume(node.data.if_node.cond));
            break;
        case IR::AbstractSyntaxNode::Type::EndIf:
            ctx.Add("}}");
            break;
        case IR::AbstractSyntaxNode::Type::Break:
            if (node.data.break_node.cond.IsImmediate()) {
                if (node.data.break_node.cond.U1()) {
                    ctx.Add("break;");
                }
            } else {
                ctx.Add("if({}){{break;}}", ctx.var_alloc.Consume(node.data.break_node.cond));
            }
            break;
        case IR::AbstractSyntaxNode::Type::Return:
        case IR::AbstractSyntaxNode::Type::Unreachable:
            ctx.Add("return;");
            break;
        case IR::AbstractSyntaxNode::Type::Loop:
            ctx.Add("for(;;){{");
            break;
        case IR::AbstractSyntaxNode::Type::Repeat:
            if (Settings::values.disable_shader_loop_safety_checks) {
                ctx.Add("if(!{}){{break;}}}}", ctx.var_alloc.Consume(node.data.repeat.cond));
            } else {
                ctx.Add("if(--loop{}<0 || !{}){{break;}}}}", ctx.num_safety_loop_vars++,
                        ctx.var_alloc.Consume(node.data.repeat.cond));
            }
            break;
        default:
            throw NotImplementedException("AbstractSyntaxNode Type {}", node.type);
        }
    }
}

std::string GlslVersionSpecifier(const EmitContext& ctx) {
    if (ctx.uses_y_direction || ctx.info.stores.Legacy() || ctx.info.loads.Legacy()) {
        return " compatibility";
    }
    return "";
}

bool IsPreciseType(GlslVarType type) {
    switch (type) {
    case GlslVarType::PrecF32:
    case GlslVarType::PrecF64:
        return true;
    default:
        return false;
    }
}

void DefineVariables(const EmitContext& ctx, std::string& header) {
    for (u32 i = 0; i < static_cast<u32>(GlslVarType::Void); ++i) {
        const auto type{static_cast<GlslVarType>(i)};
        const auto& tracker{ctx.var_alloc.GetUseTracker(type)};
        const auto type_name{ctx.var_alloc.GetGlslType(type)};
        const bool has_precise_bug{ctx.stage == Stage::Fragment && ctx.profile.has_gl_precise_bug};
        const auto precise{!has_precise_bug && IsPreciseType(type) ? "precise " : ""};
        // Temps/return types that are never used are stored at index 0
        if (tracker.uses_temp) {
            header += fmt::format("{}{} t{}={}(0);", precise, type_name,
                                  ctx.var_alloc.Representation(0, type), type_name);
        }
        for (u32 index = 0; index < tracker.num_used; ++index) {
            header += fmt::format("{}{} {}={}(0);", precise, type_name,
                                  ctx.var_alloc.Representation(index, type), type_name);
        }
    }
    for (u32 i = 0; i < ctx.num_safety_loop_vars; ++i) {
        header += fmt::format("int loop{}=0x2000;", i);
    }
}
} // Anonymous namespace

std::string EmitGLSL(const Profile& profile, const RuntimeInfo& runtime_info, IR::Program& program,
                     Bindings& bindings) {
    EmitContext ctx{program, bindings, profile, runtime_info};
    Precolor(program);
    EmitCode(ctx, program);
    const std::string version{fmt::format("#version 450{}\n", GlslVersionSpecifier(ctx))};
    ctx.header.insert(0, version);
    if (program.shared_memory_size > 0) {
        ctx.header +=
            fmt::format("shared uint smem[{}];", Common::DivCeil(program.shared_memory_size, 4U));
    }
    ctx.header += "void main(){\n";
    if (program.local_memory_size > 0) {
        ctx.header += fmt::format("uint lmem[{}];", Common::DivCeil(program.local_memory_size, 4U));
    }
    DefineVariables(ctx, ctx.header);
    if (ctx.uses_cc_carry) {
        ctx.header += "uint carry;";
    }
    if (program.info.uses_subgroup_shuffles) {
        ctx.header += "bool shfl_in_bounds;";
    }
    ctx.code.insert(0, ctx.header);
    ctx.code += '}';
    return ctx.code;
}

} // namespace Shader::Backend::GLSL
