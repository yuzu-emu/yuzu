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
}

template <auto method>
static void Invoke(EmitSPIRV& emit, EmitContext& ctx, IR::Inst* inst) {
    using M = decltype(method);
    using std::is_invocable_r_v;
    if constexpr (is_invocable_r_v<Id, M, EmitSPIRV&, EmitContext&>) {
        ctx.Define(inst, (emit.*method)(ctx));
    } else if constexpr (is_invocable_r_v<Id, M, EmitSPIRV&, EmitContext&, Id>) {
        ctx.Define(inst, (emit.*method)(ctx, ctx.Def(inst->Arg(0))));
    } else if constexpr (is_invocable_r_v<Id, M, EmitSPIRV&, EmitContext&, Id, Id>) {
        ctx.Define(inst, (emit.*method)(ctx, ctx.Def(inst->Arg(0)), ctx.Def(inst->Arg(1))));
    } else if constexpr (is_invocable_r_v<Id, M, EmitSPIRV&, EmitContext&, Id, Id, Id>) {
        ctx.Define(inst, (emit.*method)(ctx, ctx.Def(inst->Arg(0)), ctx.Def(inst->Arg(1)),
                                        ctx.Def(inst->Arg(2))));
    } else if constexpr (is_invocable_r_v<Id, M, EmitSPIRV&, EmitContext&, IR::Inst*, Id, Id>) {
        ctx.Define(inst, (emit.*method)(ctx, inst, ctx.Def(inst->Arg(0)), ctx.Def(inst->Arg(1))));
    } else if constexpr (is_invocable_r_v<Id, M, EmitSPIRV&, EmitContext&, IR::Inst*, Id, Id, Id>) {
        ctx.Define(inst, (emit.*method)(ctx, inst, ctx.Def(inst->Arg(0)), ctx.Def(inst->Arg(1)),
                                        ctx.Def(inst->Arg(2))));
    } else if constexpr (is_invocable_r_v<Id, M, EmitSPIRV&, EmitContext&, Id, u32>) {
        ctx.Define(inst, (emit.*method)(ctx, ctx.Def(inst->Arg(0)), inst->Arg(1).U32()));
    } else if constexpr (is_invocable_r_v<Id, M, EmitSPIRV&, EmitContext&, const IR::Value&>) {
        ctx.Define(inst, (emit.*method)(ctx, inst->Arg(0)));
    } else if constexpr (is_invocable_r_v<Id, M, EmitSPIRV&, EmitContext&, const IR::Value&,
                                          const IR::Value&>) {
        ctx.Define(inst, (emit.*method)(ctx, inst->Arg(0), inst->Arg(1)));
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

void EmitSPIRV::EmitPhi(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitVoid(EmitContext&) {}

void EmitSPIRV::EmitIdentity(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
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
