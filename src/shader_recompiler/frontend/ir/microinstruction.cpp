// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>

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

Inst::Inst(IR::Opcode op_, u64 flags_) noexcept : op{op_}, flags{flags_} {
    if (op == Opcode::Phi) {
        std::construct_at(&phi_args);
    } else {
        std::construct_at(&args);
    }
}

Inst::~Inst() {
    if (op == Opcode::Phi) {
        std::destroy_at(&phi_args);
    } else {
        std::destroy_at(&args);
    }
}

bool Inst::MayHaveSideEffects() const noexcept {
    switch (op) {
    case Opcode::Branch:
    case Opcode::BranchConditional:
    case Opcode::LoopMerge:
    case Opcode::SelectionMerge:
    case Opcode::Return:
    case Opcode::SetAttribute:
    case Opcode::SetAttributeIndexed:
    case Opcode::WriteGlobalU8:
    case Opcode::WriteGlobalS8:
    case Opcode::WriteGlobalU16:
    case Opcode::WriteGlobalS16:
    case Opcode::WriteGlobal32:
    case Opcode::WriteGlobal64:
    case Opcode::WriteGlobal128:
    case Opcode::WriteStorageU8:
    case Opcode::WriteStorageS8:
    case Opcode::WriteStorageU16:
    case Opcode::WriteStorageS16:
    case Opcode::WriteStorage32:
    case Opcode::WriteStorage64:
    case Opcode::WriteStorage128:
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
        return true;
    default:
        return false;
    }
}

bool Inst::AreAllArgsImmediates() const {
    if (op == Opcode::Phi) {
        throw LogicError("Testing for all arguments are immediates on phi instruction");
    }
    return std::all_of(args.begin(), args.begin() + NumArgs(),
                       [](const IR::Value& value) { return value.IsImmediate(); });
}

bool Inst::HasAssociatedPseudoOperation() const noexcept {
    return zero_inst || sign_inst || carry_inst || overflow_inst;
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
    default:
        throw InvalidArgument("{} is not a pseudo-instruction", opcode);
    }
}

size_t Inst::NumArgs() const {
    return op == Opcode::Phi ? phi_args.size() : NumArgsOf(op);
}

IR::Type Inst::Type() const {
    return TypeOf(op);
}

Value Inst::Arg(size_t index) const {
    if (op == Opcode::Phi) {
        if (index >= phi_args.size()) {
            throw InvalidArgument("Out of bounds argument index {} in phi instruction", index);
        }
        return phi_args[index].second;
    } else {
        if (index >= NumArgsOf(op)) {
            throw InvalidArgument("Out of bounds argument index {} in opcode {}", index, op);
        }
        return args[index];
    }
}

void Inst::SetArg(size_t index, Value value) {
    if (op == Opcode::Phi) {
        throw LogicError("Setting argument on a phi instruction");
    }
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

Block* Inst::PhiBlock(size_t index) const {
    if (op != Opcode::Phi) {
        throw LogicError("{} is not a Phi instruction", op);
    }
    if (index >= phi_args.size()) {
        throw InvalidArgument("Out of bounds argument index {} in phi instruction");
    }
    return phi_args[index].first;
}

void Inst::AddPhiOperand(Block* predecessor, const Value& value) {
    if (!value.IsImmediate()) {
        Use(value);
    }
    phi_args.emplace_back(predecessor, value);
}

void Inst::Invalidate() {
    ClearArgs();
    op = Opcode::Void;
}

void Inst::ClearArgs() {
    if (op == Opcode::Phi) {
        for (auto& pair : phi_args) {
            IR::Value& value{pair.second};
            if (!value.IsImmediate()) {
                UndoUse(value);
            }
        }
        phi_args.clear();
    } else {
        for (auto& value : args) {
            if (!value.IsImmediate()) {
                UndoUse(value);
            }
            value = {};
        }
    }
}

void Inst::ReplaceUsesWith(Value replacement) {
    Invalidate();

    op = Opcode::Identity;

    if (!replacement.IsImmediate()) {
        Use(replacement);
    }
    if (op == Opcode::Phi) {
        phi_args[0].second = replacement;
    } else {
        args[0] = replacement;
    }
}

void Inst::Use(const Value& value) {
    Inst* const inst{value.Inst()};
    ++inst->use_count;

    switch (op) {
    case Opcode::GetZeroFromOp:
        SetPseudoInstruction(inst->zero_inst, this);
        break;
    case Opcode::GetSignFromOp:
        SetPseudoInstruction(inst->sign_inst, this);
        break;
    case Opcode::GetCarryFromOp:
        SetPseudoInstruction(inst->carry_inst, this);
        break;
    case Opcode::GetOverflowFromOp:
        SetPseudoInstruction(inst->overflow_inst, this);
        break;
    default:
        break;
    }
}

void Inst::UndoUse(const Value& value) {
    Inst* const inst{value.Inst()};
    --inst->use_count;

    switch (op) {
    case Opcode::GetZeroFromOp:
        RemovePseudoInstruction(inst->zero_inst, Opcode::GetZeroFromOp);
        break;
    case Opcode::GetSignFromOp:
        RemovePseudoInstruction(inst->sign_inst, Opcode::GetSignFromOp);
        break;
    case Opcode::GetCarryFromOp:
        RemovePseudoInstruction(inst->carry_inst, Opcode::GetCarryFromOp);
        break;
    case Opcode::GetOverflowFromOp:
        RemovePseudoInstruction(inst->overflow_inst, Opcode::GetOverflowFromOp);
        break;
    default:
        break;
    }
}

} // namespace Shader::IR
