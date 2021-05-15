// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <ranges>
#include <string>
#include <tuple>

#include "shader_recompiler/backend/bindings.h"
#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::GLASM {
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

template <typename T>
struct Identity {
    Identity(T data_) : data{data_} {}

    T Extract() {
        return data;
    }

    T data;
};

template <bool scalar>
class RegWrapper {
public:
    RegWrapper(EmitContext& ctx, const IR::Value& ir_value) : reg_alloc{ctx.reg_alloc} {
        const Value value{reg_alloc.Peek(ir_value)};
        if (value.type == Type::Register) {
            inst = ir_value.InstRecursive();
            reg = Register{value};
        } else {
            const bool is_long{value.type == Type::F64 || value.type == Type::U64};
            reg = is_long ? reg_alloc.AllocLongReg() : reg_alloc.AllocReg();
        }
        switch (value.type) {
        case Type::Register:
        case Type::Void:
            break;
        case Type::U32:
            ctx.Add("MOV.U {}.x,{};", reg, value.imm_u32);
            break;
        case Type::S32:
            ctx.Add("MOV.S {}.x,{};", reg, value.imm_s32);
            break;
        case Type::F32:
            ctx.Add("MOV.F {}.x,{};", reg, value.imm_f32);
            break;
        case Type::U64:
            ctx.Add("MOV.U64 {}.x,{};", reg, value.imm_u64);
            break;
        case Type::F64:
            ctx.Add("MOV.F64 {}.x,{};", reg, value.imm_f64);
            break;
        }
    }

    auto Extract() {
        if (inst) {
            reg_alloc.Unref(*inst);
        } else {
            reg_alloc.FreeReg(reg);
        }
        return std::conditional_t<scalar, ScalarRegister, Register>{Value{reg}};
    }

private:
    RegAlloc& reg_alloc;
    IR::Inst* inst{};
    Register reg{};
};

template <typename ArgType>
class ValueWrapper {
public:
    ValueWrapper(EmitContext& ctx, const IR::Value& ir_value_)
        : reg_alloc{ctx.reg_alloc}, ir_value{ir_value_}, value{reg_alloc.Peek(ir_value)} {}

    ArgType Extract() {
        if (!ir_value.IsImmediate()) {
            reg_alloc.Unref(*ir_value.InstRecursive());
        }
        return value;
    }

private:
    RegAlloc& reg_alloc;
    const IR::Value& ir_value;
    ArgType value;
};

template <typename ArgType>
auto Arg(EmitContext& ctx, const IR::Value& arg) {
    if constexpr (std::is_same_v<ArgType, Register>) {
        return RegWrapper<false>{ctx, arg};
    } else if constexpr (std::is_same_v<ArgType, ScalarRegister>) {
        return RegWrapper<true>{ctx, arg};
    } else if constexpr (std::is_base_of_v<Value, ArgType>) {
        return ValueWrapper<ArgType>{ctx, arg};
    } else if constexpr (std::is_same_v<ArgType, const IR::Value&>) {
        return Identity<const IR::Value&>{arg};
    } else if constexpr (std::is_same_v<ArgType, u32>) {
        return Identity{arg.U32()};
    } else if constexpr (std::is_same_v<ArgType, IR::Attribute>) {
        return Identity{arg.Attribute()};
    } else if constexpr (std::is_same_v<ArgType, IR::Patch>) {
        return Identity{arg.Patch()};
    } else if constexpr (std::is_same_v<ArgType, IR::Reg>) {
        return Identity{arg.Reg()};
    }
}

template <auto func, bool is_first_arg_inst>
struct InvokeCall {
    template <typename... Args>
    InvokeCall(EmitContext& ctx, IR::Inst* inst, Args&&... args) {
        if constexpr (is_first_arg_inst) {
            func(ctx, *inst, args.Extract()...);
        } else {
            func(ctx, args.Extract()...);
        }
    }
};

template <auto func, bool is_first_arg_inst, size_t... I>
void Invoke(EmitContext& ctx, IR::Inst* inst, std::index_sequence<I...>) {
    using Traits = FuncTraits<decltype(func)>;
    if constexpr (is_first_arg_inst) {
        InvokeCall<func, is_first_arg_inst>{
            ctx, inst, Arg<typename Traits::template ArgType<I + 2>>(ctx, inst->Arg(I))...};
    } else {
        InvokeCall<func, is_first_arg_inst>{
            ctx, inst, Arg<typename Traits::template ArgType<I + 1>>(ctx, inst->Arg(I))...};
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

void Precolor(EmitContext& ctx, const IR::Program& program) {
    for (IR::Block* const block : program.blocks) {
        for (IR::Inst& phi : block->Instructions() | std::views::take_while(IR::IsPhi)) {
            switch (phi.Arg(0).Type()) {
            case IR::Type::U1:
            case IR::Type::U32:
            case IR::Type::F32:
                ctx.reg_alloc.Define(phi);
                break;
            case IR::Type::U64:
            case IR::Type::F64:
                ctx.reg_alloc.LongDefine(phi);
                break;
            default:
                throw NotImplementedException("Phi node type {}", phi.Type());
            }
            const size_t num_args{phi.NumArgs()};
            for (size_t i = 0; i < num_args; ++i) {
                IR::IREmitter{*phi.PhiBlock(i)}.PhiMove(phi, phi.Arg(i));
            }
            // Add reference to the phi node on the phi predecessor to avoid overwritting it
            for (size_t i = 0; i < num_args; ++i) {
                IR::IREmitter{*phi.PhiBlock(i)}.DummyReference(IR::Value{&phi});
            }
        }
    }
}

void EmitCode(EmitContext& ctx, const IR::Program& program) {
    const auto eval{
        [&](const IR::U1& cond) { return ScalarS32{ctx.reg_alloc.Consume(IR::Value{cond})}; }};
    for (const IR::AbstractSyntaxNode& node : program.syntax_list) {
        switch (node.type) {
        case IR::AbstractSyntaxNode::Type::Block:
            for (IR::Inst& inst : node.block->Instructions()) {
                EmitInst(ctx, &inst);
            }
            break;
        case IR::AbstractSyntaxNode::Type::If:
            ctx.Add("MOV.S.CC RC,{};"
                    "IF NE.x;",
                    eval(node.if_node.cond));
            break;
        case IR::AbstractSyntaxNode::Type::EndIf:
            ctx.Add("ENDIF;");
            break;
        case IR::AbstractSyntaxNode::Type::Loop:
            ctx.Add("REP;");
            break;
        case IR::AbstractSyntaxNode::Type::Repeat:
            if (node.repeat.cond.IsImmediate()) {
                if (node.repeat.cond.U1()) {
                    ctx.Add("ENDREP;");
                } else {
                    ctx.Add("BRK;"
                            "ENDREP;");
                }
            } else {
                ctx.Add("MOV.S.CC RC,{};"
                        "BRK (EQ.x);"
                        "ENDREP;",
                        eval(node.repeat.cond));
            }
            break;
        case IR::AbstractSyntaxNode::Type::Break:
            if (node.break_node.cond.IsImmediate()) {
                if (node.break_node.cond.U1()) {
                    ctx.Add("BRK;");
                }
            } else {
                ctx.Add("MOV.S.CC RC,{};"
                        "BRK (NE.x);",
                        eval(node.break_node.cond));
            }
            break;
        case IR::AbstractSyntaxNode::Type::Return:
        case IR::AbstractSyntaxNode::Type::Unreachable:
            ctx.Add("RET;");
            break;
        }
    }
}

void SetupOptions(std::string& header, Info info) {
    // TODO: Track the shared atomic ops
    header += "OPTION NV_internal;"
              "OPTION NV_shader_storage_buffer;"
              "OPTION NV_gpu_program_fp64;"
              "OPTION NV_bindless_texture;"
              "OPTION ARB_derivative_control;";
    if (info.uses_int64_bit_atomics) {
        header += "OPTION NV_shader_atomic_int64;";
    }
    if (info.uses_atomic_f32_add) {
        header += "OPTION NV_shader_atomic_float;";
    }
    if (info.uses_atomic_f16x2_add || info.uses_atomic_f16x2_min || info.uses_atomic_f16x2_max) {
        header += "OPTION NV_shader_atomic_fp16_vector;";
    }
    if (info.uses_subgroup_invocation_id || info.uses_subgroup_mask || info.uses_subgroup_vote) {
        header += "OPTION NV_shader_thread_group;";
    }
    if (info.uses_subgroup_shuffles) {
        header += "OPTION NV_shader_thread_shuffle;";
    }
}

std::string_view StageHeader(Stage stage) {
    switch (stage) {
    case Stage::VertexA:
    case Stage::VertexB:
        return "!!NVvp5.0\n";
    case Stage::TessellationControl:
        return "!!NVtcs5.0\n";
    case Stage::TessellationEval:
        return "!!NVtes5.0\n";
    case Stage::Geometry:
        return "!!NVgp5.0\n";
    case Stage::Fragment:
        return "!!NVfp5.0\n";
    case Stage::Compute:
        return "!!NVcp5.0\n";
    }
    throw InvalidArgument("Invalid stage {}", stage);
}
} // Anonymous namespace

std::string EmitGLASM(const Profile&, IR::Program& program, Bindings&) {
    EmitContext ctx{program};
    Precolor(ctx, program);
    EmitCode(ctx, program);
    std::string header{StageHeader(program.stage)};
    SetupOptions(header, program.info);
    switch (program.stage) {
    case Stage::Compute:
        header += fmt::format("GROUP_SIZE {} {} {};", program.workgroup_size[0],
                              program.workgroup_size[1], program.workgroup_size[2]);
        break;
    default:
        break;
    }
    if (program.shared_memory_size > 0) {
        header += fmt::format("SHARED_MEMORY {};", program.shared_memory_size);
        header += fmt::format("SHARED shared_mem[]={{program.sharedmem}};");
    }
    header += "TEMP ";
    for (size_t index = 0; index < ctx.reg_alloc.NumUsedRegisters(); ++index) {
        header += fmt::format("R{},", index);
    }
    if (program.local_memory_size > 0) {
        header += fmt::format("lmem[{}],", program.local_memory_size);
    }
    header += "RC;"
              "LONG TEMP ";
    for (size_t index = 0; index < ctx.reg_alloc.NumUsedLongRegisters(); ++index) {
        header += fmt::format("D{},", index);
    }
    header += "DC;";
    ctx.code.insert(0, header);
    ctx.code += "END";
    return ctx.code;
}

} // namespace Shader::Backend::GLASM
