// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>
#include <tuple>

#include "shader_recompiler/backend/bindings.h"
#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
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
    Identity(const T& data_) : data{data_} {}

    const T& Extract() {
        return data;
    }

    T data;
};

template <bool scalar>
struct RegWrapper {
    RegWrapper(EmitContext& ctx, Value value)
        : reg_alloc{ctx.reg_alloc}, allocated{value.type != Type::Register} {
        reg = allocated ? reg_alloc.AllocReg() : Register{value};
        switch (value.type) {
        case Type::Register:
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
        }
    }
    ~RegWrapper() {
        if (allocated) {
            reg_alloc.FreeReg(reg);
        }
    }

    auto Extract() {
        return std::conditional_t<scalar, ScalarRegister, Register>{Value{reg}};
    }

    RegAlloc& reg_alloc;
    Register reg{};
    bool allocated{};
};

template <typename ArgType>
auto Arg(EmitContext& ctx, const IR::Value& arg) {
    if constexpr (std::is_same_v<ArgType, Register>) {
        return RegWrapper<false>{ctx, ctx.reg_alloc.Consume(arg)};
    } else if constexpr (std::is_same_v<ArgType, ScalarRegister>) {
        return RegWrapper<true>{ctx, ctx.reg_alloc.Consume(arg)};
    } else if constexpr (std::is_base_of_v<Value, ArgType>) {
        return Identity{ArgType{ctx.reg_alloc.Consume(arg)}};
    } else if constexpr (std::is_same_v<ArgType, const IR::Value&>) {
        return Identity{arg};
    } else if constexpr (std::is_same_v<ArgType, u32>) {
        return Identity{arg.U32()};
    } else if constexpr (std::is_same_v<ArgType, IR::Block*>) {
        return Identity{arg.Label()};
    } else if constexpr (std::is_same_v<ArgType, IR::Attribute>) {
        return Identity{arg.Attribute()};
    } else if constexpr (std::is_same_v<ArgType, IR::Patch>) {
        return Identity{arg.Patch()};
    } else if constexpr (std::is_same_v<ArgType, IR::Reg>) {
        return Identity{arg.Reg()};
    }
}

template <auto func, bool is_first_arg_inst, typename... Args>
void InvokeCall(EmitContext& ctx, IR::Inst* inst, Args&&... args) {
    if constexpr (is_first_arg_inst) {
        func(ctx, *inst, std::forward<Args>(args.Extract())...);
    } else {
        func(ctx, std::forward<Args>(args.Extract())...);
    }
}

template <auto func, bool is_first_arg_inst, size_t... I>
void Invoke(EmitContext& ctx, IR::Inst* inst, std::index_sequence<I...>) {
    using Traits = FuncTraits<decltype(func)>;
    if constexpr (is_first_arg_inst) {
        func(ctx, *inst,
             Arg<typename Traits::template ArgType<I + 2>>(ctx, inst->Arg(I)).Extract()...);
    } else {
        func(ctx, Arg<typename Traits::template ArgType<I + 1>>(ctx, inst->Arg(I)).Extract()...);
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

void Alias(IR::Inst& inst, const IR::Value& value) {
    if (value.IsImmediate()) {
        return;
    }
    IR::Inst* const value_inst{value.InstRecursive()};
    if (inst.GetOpcode() == IR::Opcode::Identity) {
        value_inst->DestructiveAddUsage(inst.UseCount());
        value_inst->DestructiveRemoveUsage();
    }
    inst.SetDefinition(value_inst->Definition<Id>());
}
} // Anonymous namespace

std::string EmitGLASM(const Profile&, IR::Program& program, Bindings&) {
    EmitContext ctx{program};
    for (IR::Block* const block : program.blocks) {
        for (IR::Inst& inst : block->Instructions()) {
            EmitInst(ctx, &inst);
        }
    }
    std::string header = "!!NVcp5.0\n"
                         "OPTION NV_internal;";
    switch (program.stage) {
    case Stage::Compute:
        header += fmt::format("GROUP_SIZE {} {} {};", program.workgroup_size[0],
                              program.workgroup_size[1], program.workgroup_size[2]);
        break;
    default:
        break;
    }
    header += "TEMP ";
    for (size_t index = 0; index < ctx.reg_alloc.NumUsedRegisters(); ++index) {
        header += fmt::format("R{},", index);
    }
    header += "RC;";
    if (!program.info.storage_buffers_descriptors.empty()) {
        header += "LONG TEMP LC;";
    }
    ctx.code.insert(0, header);
    ctx.code += "END";
    return ctx.code;
}

void EmitIdentity(EmitContext&, IR::Inst& inst, const IR::Value& value) {
    Alias(inst, value);
}

void EmitBitCastU16F16(EmitContext&, IR::Inst& inst, const IR::Value& value) {
    Alias(inst, value);
}

void EmitBitCastU32F32(EmitContext&, IR::Inst& inst, const IR::Value& value) {
    Alias(inst, value);
}

void EmitBitCastU64F64(EmitContext&, IR::Inst& inst, const IR::Value& value) {
    Alias(inst, value);
}

void EmitBitCastF16U16(EmitContext&, IR::Inst& inst, const IR::Value& value) {
    Alias(inst, value);
}

void EmitBitCastF32U32(EmitContext&, IR::Inst& inst, const IR::Value& value) {
    Alias(inst, value);
}

void EmitBitCastF64U64(EmitContext&, IR::Inst& inst, const IR::Value& value) {
    Alias(inst, value);
}

} // namespace Shader::Backend::GLASM
