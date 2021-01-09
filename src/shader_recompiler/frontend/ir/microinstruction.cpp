// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/ir/microinstruction.h"
#include "shader_recompiler/frontend/ir/type.h"

namespace Shader::IR {

static void CheckPseudoInstruction(IR::Inst* inst, IR::Opcode opcode) {
    if (inst && inst->Opcode() != opcode) {
        throw LogicError("Invalid pseudo-instruction");
    }
}

static void SetPseudoInstruction(IR::Inst*& dest_inst, IR::Inst* pseudo_inst) {
    if (dest_inst) {
        throw LogicError("Only one of each type of pseudo-op allowed");
    }
    dest_inst = pseudo_inst;
}

static void RemovePseudoInstruction(IR::Inst*& inst, IR::Opcode expected_opcode) {
    if (inst->Opcode() != expected_opcode) {
        throw LogicError("Undoing use of invalid pseudo-op");
    }
    inst = nullptr;
}

bool Inst::MayHaveSideEffects() const noexcept {
    switch (op) {
    case Opcode::SetAttribute:
    case Opcode::SetAttributeIndexed:
    case Opcode::WriteGlobalU8:
    case Opcode::WriteGlobalS8:
    case Opcode::WriteGlobalU16:
    case Opcode::WriteGlobalS16:
    case Opcode::WriteGlobal32:
    case Opcode::WriteGlobal64:
    case Opcode::WriteGlobal128:
        return true;
    default:
        return false;
    }
}

bool Inst::IsPseudoInstruction() const noexcept {
    switch (op) {
    case Opcode::GetZeroFromOp:
    case Opcode::GetSignFromOp:
    case Opcode::GetCarryFromOp:
    case Opcode::GetOverflowFromOp:
    case Opcode::GetZSCOFromOp:
        return true;
    default:
        return false;
    }
}

bool Inst::HasAssociatedPseudoOperation() const noexcept {
    return zero_inst || sign_inst || carry_inst || overflow_inst || zsco_inst;
}

Inst* Inst::GetAssociatedPseudoOperation(IR::Opcode opcode) {
    // This is faster than doing a search through the block.
    switch (opcode) {
    case Opcode::GetZeroFromOp:
        CheckPseudoInstruction(zero_inst, Opcode::GetZeroFromOp);
        return zero_inst;
    case Opcode::GetSignFromOp:
        CheckPseudoInstruction(sign_inst, Opcode::GetSignFromOp);
        return sign_inst;
    case Opcode::GetCarryFromOp:
        CheckPseudoInstruction(carry_inst, Opcode::GetCarryFromOp);
        return carry_inst;
    case Opcode::GetOverflowFromOp:
        CheckPseudoInstruction(overflow_inst, Opcode::GetOverflowFromOp);
        return overflow_inst;
    case Opcode::GetZSCOFromOp:
        CheckPseudoInstruction(zsco_inst, Opcode::GetZSCOFromOp);
        return zsco_inst;
    default:
        throw InvalidArgument("{} is not a pseudo-instruction", opcode);
    }
}

size_t Inst::NumArgs() const {
    return NumArgsOf(op);
}

IR::Type Inst::Type() const {
    return TypeOf(op);
}

Value Inst::Arg(size_t index) const {
    if (index >= NumArgsOf(op)) {
        throw InvalidArgument("Out of bounds argument index {} in opcode {}", index, op);
    }
    return args[index];
}

void Inst::SetArg(size_t index, Value value) {
    if (index >= NumArgsOf(op)) {
        throw InvalidArgument("Out of bounds argument index {} in opcode {}", index, op);
    }
    if (!args[index].IsImmediate()) {
        UndoUse(args[index]);
    }
    if (!value.IsImmediate()) {
        Use(value);
    }
    args[index] = value;
}

void Inst::Invalidate() {
    ClearArgs();
    op = Opcode::Void;
}

void Inst::ClearArgs() {
    for (auto& value : args) {
        if (!value.IsImmediate()) {
            UndoUse(value);
        }
        value = {};
    }
}

void Inst::ReplaceUsesWith(Value replacement) {
    Invalidate();

    op = Opcode::Identity;

    if (!replacement.IsImmediate()) {
        Use(replacement);
    }
    args[0] = replacement;
}

void Inst::Use(const Value& value) {
    ++value.Inst()->use_count;

    switch (op) {
    case Opcode::GetZeroFromOp:
        SetPseudoInstruction(value.Inst()->zero_inst, this);
        break;
    case Opcode::GetSignFromOp:
        SetPseudoInstruction(value.Inst()->sign_inst, this);
        break;
    case Opcode::GetCarryFromOp:
        SetPseudoInstruction(value.Inst()->carry_inst, this);
        break;
    case Opcode::GetOverflowFromOp:
        SetPseudoInstruction(value.Inst()->overflow_inst, this);
        break;
    case Opcode::GetZSCOFromOp:
        SetPseudoInstruction(value.Inst()->zsco_inst, this);
        break;
    default:
        break;
    }
}

void Inst::UndoUse(const Value& value) {
    --value.Inst()->use_count;

    switch (op) {
    case Opcode::GetZeroFromOp:
        RemovePseudoInstruction(value.Inst()->zero_inst, Opcode::GetZeroFromOp);
        break;
    case Opcode::GetSignFromOp:
        RemovePseudoInstruction(value.Inst()->sign_inst, Opcode::GetSignFromOp);
        break;
    case Opcode::GetCarryFromOp:
        RemovePseudoInstruction(value.Inst()->carry_inst, Opcode::GetCarryFromOp);
        break;
    case Opcode::GetOverflowFromOp:
        RemovePseudoInstruction(value.Inst()->overflow_inst, Opcode::GetOverflowFromOp);
        break;
    case Opcode::GetZSCOFromOp:
        RemovePseudoInstruction(value.Inst()->zsco_inst, Opcode::GetZSCOFromOp);
        break;
    default:
        break;
    }
}

} // namespace Shader::IR
