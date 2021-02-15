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

EmitContext::EmitContext(IR::Program& program) {
    AddCapability(spv::Capability::Shader);
    AddCapability(spv::Capability::Float16);
    AddCapability(spv::Capability::Float64);
    void_id = TypeVoid();

    u1 = Name(TypeBool(), "u1");
    f32.Define(*this, TypeFloat(32), "f32");
    u32.Define(*this, TypeInt(32, false), "u32");
    f16.Define(*this, TypeFloat(16), "f16");
    f64.Define(*this, TypeFloat(64), "f64");

    true_value = ConstantTrue(u1);
    false_value = ConstantFalse(u1);

    for (const IR::Function& function : program.functions) {
        for (IR::Block* const block : function.blocks) {
            block_label_map.emplace_back(block, OpLabel());
        }
    }
    std::ranges::sort(block_label_map, {}, &std::pair<IR::Block*, Id>::first);
}

EmitContext::~EmitContext() = default;

EmitSPIRV::EmitSPIRV(IR::Program& program) {
    EmitContext ctx{program};
    const Id void_function{ctx.TypeFunction(ctx.void_id)};
    // FIXME: Forward declare functions (needs sirit support)
    Id func{};
    for (IR::Function& function : program.functions) {
        func = ctx.OpFunction(ctx.void_id, spv::FunctionControlMask::MaskNone, void_function);
        for (IR::Block* const block : function.blocks) {
            ctx.AddLabel(ctx.BlockLabel(block));
            for (IR::Inst& inst : block->Instructions()) {
                EmitInst(ctx, &inst);
            }
        }
        ctx.OpFunctionEnd();
    }
    ctx.AddEntryPoint(spv::ExecutionModel::GLCompute, func, "main");

    std::vector<u32> result{ctx.Assemble()};
    std::FILE* file{std::fopen("shader.spv", "wb")};
    std::fwrite(result.data(), sizeof(u32), result.size(), file);
    std::fclose(file);
    std::system("spirv-dis shader.spv");
    std::system("spirv-val shader.spv");
    std::system("spirv-cross shader.spv");
}

template <auto method, typename... Args>
static void SetDefinition(EmitSPIRV& emit, EmitContext& ctx, IR::Inst* inst, Args... args) {
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

template <auto method>
static void Invoke(EmitSPIRV& emit, EmitContext& ctx, IR::Inst* inst) {
    using M = decltype(method);
    using std::is_invocable_r_v;
    if constexpr (is_invocable_r_v<Id, M, EmitSPIRV&, EmitContext&>) {
        SetDefinition<method>(emit, ctx, inst);
    } else if constexpr (is_invocable_r_v<Id, M, EmitSPIRV&, EmitContext&, Id>) {
        SetDefinition<method>(emit, ctx, inst, ctx.Def(inst->Arg(0)));
    } else if constexpr (is_invocable_r_v<Id, M, EmitSPIRV&, EmitContext&, Id, Id>) {
        SetDefinition<method>(emit, ctx, inst, ctx.Def(inst->Arg(0)), ctx.Def(inst->Arg(1)));
    } else if constexpr (is_invocable_r_v<Id, M, EmitSPIRV&, EmitContext&, Id, Id, Id>) {
        SetDefinition<method>(emit, ctx, inst, ctx.Def(inst->Arg(0)), ctx.Def(inst->Arg(1)),
                              ctx.Def(inst->Arg(2)));
    } else if constexpr (is_invocable_r_v<Id, M, EmitSPIRV&, EmitContext&, IR::Inst*>) {
        SetDefinition<method>(emit, ctx, inst, inst);
    } else if constexpr (is_invocable_r_v<Id, M, EmitSPIRV&, EmitContext&, IR::Inst*, Id, Id>) {
        SetDefinition<method>(emit, ctx, inst, inst, ctx.Def(inst->Arg(0)), ctx.Def(inst->Arg(1)));
    } else if constexpr (is_invocable_r_v<Id, M, EmitSPIRV&, EmitContext&, IR::Inst*, Id, Id, Id>) {
        SetDefinition<method>(emit, ctx, inst, inst, ctx.Def(inst->Arg(0)), ctx.Def(inst->Arg(1)),
                              ctx.Def(inst->Arg(2)));
    } else if constexpr (is_invocable_r_v<Id, M, EmitSPIRV&, EmitContext&, Id, u32>) {
        SetDefinition<method>(emit, ctx, inst, ctx.Def(inst->Arg(0)), inst->Arg(1).U32());
    } else if constexpr (is_invocable_r_v<Id, M, EmitSPIRV&, EmitContext&, const IR::Value&>) {
        SetDefinition<method>(emit, ctx, inst, inst->Arg(0));
    } else if constexpr (is_invocable_r_v<Id, M, EmitSPIRV&, EmitContext&, const IR::Value&,
                                          const IR::Value&>) {
        SetDefinition<method>(emit, ctx, inst, inst->Arg(0), inst->Arg(1));
    } else if constexpr (is_invocable_r_v<void, M, EmitSPIRV&, EmitContext&, IR::Inst*>) {
        (emit.*method)(ctx, inst);
    } else if constexpr (is_invocable_r_v<void, M, EmitSPIRV&, EmitContext&>) {
        (emit.*method)(ctx);
    } else {
        static_assert(false, "Bad format");
    }
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
        return ctx.u1;
    case IR::Type::U32:
        return ctx.u32[1];
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
        operands.push_back(ctx.BlockLabel(phi_block));
    }
    const Id result_type{TypeId(ctx, inst->Arg(0).Type())};
    return ctx.OpPhi(result_type, std::span(operands.data(), operands.size()));
}

void EmitSPIRV::EmitVoid(EmitContext&) {}

void EmitSPIRV::EmitIdentity(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

// FIXME: Move to its own file
void EmitSPIRV::EmitBranch(EmitContext& ctx, IR::Inst* inst) {
    ctx.OpBranch(ctx.BlockLabel(inst->Arg(0).Label()));
}

void EmitSPIRV::EmitBranchConditional(EmitContext& ctx, IR::Inst* inst) {
    ctx.OpBranchConditional(ctx.Def(inst->Arg(0)), ctx.BlockLabel(inst->Arg(1).Label()),
                            ctx.BlockLabel(inst->Arg(2).Label()));
}

void EmitSPIRV::EmitLoopMerge(EmitContext& ctx, IR::Inst* inst) {
    ctx.OpLoopMerge(ctx.BlockLabel(inst->Arg(0).Label()), ctx.BlockLabel(inst->Arg(1).Label()),
                    spv::LoopControlMask::MaskNone);
}

void EmitSPIRV::EmitSelectionMerge(EmitContext& ctx, IR::Inst* inst) {
    ctx.OpSelectionMerge(ctx.BlockLabel(inst->Arg(0).Label()), spv::SelectionControlMask::MaskNone);
}

void EmitSPIRV::EmitReturn(EmitContext& ctx) {
    ctx.OpReturn();
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
