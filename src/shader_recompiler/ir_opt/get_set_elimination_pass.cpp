// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>

#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/microinstruction.h"
#include "shader_recompiler/ir_opt/passes.h"

namespace Shader::Optimization {
namespace {
using Iterator = IR::Block::iterator;

enum class TrackingType {
    Reg,
};

struct RegisterInfo {
    IR::Value register_value;
    TrackingType tracking_type;
    Iterator last_set_instruction;
    bool set_instruction_present = false;
};

void DoSet(IR::Block& block, RegisterInfo& info, IR::Value value, Iterator set_inst,
           TrackingType tracking_type) {
    if (info.set_instruction_present) {
        info.last_set_instruction->Invalidate();
        block.Instructions().erase(info.last_set_instruction);
    }
    info.register_value = value;
    info.tracking_type = tracking_type;
    info.set_instruction_present = true;
    info.last_set_instruction = set_inst;
}

RegisterInfo Nothing(Iterator get_inst, TrackingType tracking_type) {
    RegisterInfo info{};
    info.register_value = IR::Value{&*get_inst};
    info.tracking_type = tracking_type;
    return info;
}

void DoGet(RegisterInfo& info, Iterator get_inst, TrackingType tracking_type) {
    if (info.register_value.IsEmpty()) {
        info = Nothing(get_inst, tracking_type);
        return;
    }
    if (info.tracking_type == tracking_type) {
        get_inst->ReplaceUsesWith(info.register_value);
        return;
    }
    info = Nothing(get_inst, tracking_type);
}
} // Anonymous namespace

void GetSetElimination(IR::Block& block) {
    std::array<RegisterInfo, 255> reg_info;

    for (Iterator inst = block.begin(); inst != block.end(); ++inst) {
        switch (inst->Opcode()) {
        case IR::Opcode::GetRegister: {
            const IR::Reg reg{inst->Arg(0).Reg()};
            if (reg == IR::Reg::RZ) {
                break;
            }
            const size_t index{static_cast<size_t>(reg)};
            DoGet(reg_info.at(index), inst, TrackingType::Reg);
            break;
        }
        case IR::Opcode::SetRegister: {
            const IR::Reg reg{inst->Arg(0).Reg()};
            if (reg == IR::Reg::RZ) {
                break;
            }
            const size_t index{static_cast<size_t>(reg)};
            DoSet(block, reg_info.at(index), inst->Arg(1), inst, TrackingType::Reg);
            break;
        }
        default:
            break;
        }
    }
}

} // namespace Shader::Optimization
