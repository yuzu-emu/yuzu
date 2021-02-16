// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/shader_info.h"

namespace Shader::Optimization {
namespace {
void AddConstantBufferDescriptor(Info& info, u32 index) {
    auto& descriptor{info.constant_buffers.at(index)};
    if (descriptor) {
        return;
    }
    descriptor = &info.constant_buffer_descriptors.emplace_back(Info::ConstantBufferDescriptor{
        .index{index},
        .count{1},
    });
}

void Visit(Info& info, IR::Inst& inst) {
    switch (inst.Opcode()) {
    case IR::Opcode::WorkgroupId:
        info.uses_workgroup_id = true;
        break;
    case IR::Opcode::LocalInvocationId:
        info.uses_local_invocation_id = true;
        break;
    case IR::Opcode::FPAbs16:
    case IR::Opcode::FPAdd16:
    case IR::Opcode::FPCeil16:
    case IR::Opcode::FPFloor16:
    case IR::Opcode::FPFma16:
    case IR::Opcode::FPMul16:
    case IR::Opcode::FPNeg16:
    case IR::Opcode::FPRoundEven16:
    case IR::Opcode::FPSaturate16:
    case IR::Opcode::FPTrunc16:
        info.uses_fp16;
        break;
    case IR::Opcode::FPAbs64:
    case IR::Opcode::FPAdd64:
    case IR::Opcode::FPCeil64:
    case IR::Opcode::FPFloor64:
    case IR::Opcode::FPFma64:
    case IR::Opcode::FPMax64:
    case IR::Opcode::FPMin64:
    case IR::Opcode::FPMul64:
    case IR::Opcode::FPNeg64:
    case IR::Opcode::FPRecip64:
    case IR::Opcode::FPRecipSqrt64:
    case IR::Opcode::FPRoundEven64:
    case IR::Opcode::FPSaturate64:
    case IR::Opcode::FPTrunc64:
        info.uses_fp64 = true;
        break;
    case IR::Opcode::GetCbuf:
        if (const IR::Value index{inst.Arg(0)}; index.IsImmediate()) {
            AddConstantBufferDescriptor(info, index.U32());
        } else {
            throw NotImplementedException("Constant buffer with non-immediate index");
        }
        break;
    default:
        break;
    }
}
} // Anonymous namespace

void CollectShaderInfoPass(IR::Program& program) {
    Info& info{program.info};
    for (IR::Function& function : program.functions) {
        for (IR::Block* const block : function.post_order_blocks) {
            for (IR::Inst& inst : block->Instructions()) {
                Visit(info, inst);
            }
        }
    }
}

} // namespace Shader::Optimization
