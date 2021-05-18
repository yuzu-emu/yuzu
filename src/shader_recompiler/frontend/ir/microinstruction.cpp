// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>

#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/ir/type.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::IR {
namespace {
void CheckPseudoInstruction(IR::Inst* inst, IR::Opcode opcode) {
    if (inst && inst->GetOpcode() != opcode) {
        throw LogicError("Invalid pseudo-instruction");
    }
}

void SetPseudoInstruction(IR::Inst*& dest_inst, IR::Inst* pseudo_inst) {
    if (dest_inst) {
        throw LogicError("Only one of each type of pseudo-op allowed");
    }
    dest_inst = pseudo_inst;
}

void RemovePseudoInstruction(IR::Inst*& inst, IR::Opcode expected_opcode) {
    if (inst->GetOpcode() != expected_opcode) {
        throw LogicError("Undoing use of invalid pseudo-op");
    }
    inst = nullptr;
}

void AllocAssociatedInsts(std::unique_ptr<AssociatedInsts>& associated_insts) {
    if (!associated_insts) {
        associated_insts = std::make_unique<AssociatedInsts>();
    }
}
} // Anonymous namespace

Inst::Inst(IR::Opcode op_, u32 flags_) noexcept : op{op_}, flags{flags_} {
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
    case Opcode::ConditionRef:
    case Opcode::Reference:
    case Opcode::PhiMove:
    case Opcode::Prologue:
    case Opcode::Epilogue:
    case Opcode::Join:
    case Opcode::DemoteToHelperInvocation:
    case Opcode::Barrier:
    case Opcode::WorkgroupMemoryBarrier:
    case Opcode::DeviceMemoryBarrier:
    case Opcode::EmitVertex:
    case Opcode::EndPrimitive:
    case Opcode::SetAttribute:
    case Opcode::SetAttributeIndexed:
    case Opcode::SetPatch:
    case Opcode::SetFragColor:
    case Opcode::SetSampleMask:
    case Opcode::SetFragDepth:
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
    case Opcode::WriteLocal:
    case Opcode::WriteSharedU8:
    case Opcode::WriteSharedU16:
    case Opcode::WriteSharedU32:
    case Opcode::WriteSharedU64:
    case Opcode::WriteSharedU128:
    case Opcode::SharedAtomicIAdd32:
    case Opcode::SharedAtomicSMin32:
    case Opcode::SharedAtomicUMin32:
    case Opcode::SharedAtomicSMax32:
    case Opcode::SharedAtomicUMax32:
    case Opcode::SharedAtomicInc32:
    case Opcode::SharedAtomicDec32:
    case Opcode::SharedAtomicAnd32:
    case Opcode::SharedAtomicOr32:
    case Opcode::SharedAtomicXor32:
    case Opcode::SharedAtomicExchange32:
    case Opcode::SharedAtomicExchange64:
    case Opcode::GlobalAtomicIAdd32:
    case Opcode::GlobalAtomicSMin32:
    case Opcode::GlobalAtomicUMin32:
    case Opcode::GlobalAtomicSMax32:
    case Opcode::GlobalAtomicUMax32:
    case Opcode::GlobalAtomicInc32:
    case Opcode::GlobalAtomicDec32:
    case Opcode::GlobalAtomicAnd32:
    case Opcode::GlobalAtomicOr32:
    case Opcode::GlobalAtomicXor32:
    case Opcode::GlobalAtomicExchange32:
    case Opcode::GlobalAtomicIAdd64:
    case Opcode::GlobalAtomicSMin64:
    case Opcode::GlobalAtomicUMin64:
    case Opcode::GlobalAtomicSMax64:
    case Opcode::GlobalAtomicUMax64:
    case Opcode::GlobalAtomicAnd64:
    case Opcode::GlobalAtomicOr64:
    case Opcode::GlobalAtomicXor64:
    case Opcode::GlobalAtomicExchange64:
    case Opcode::GlobalAtomicAddF32:
    case Opcode::GlobalAtomicAddF16x2:
    case Opcode::GlobalAtomicAddF32x2:
    case Opcode::GlobalAtomicMinF16x2:
    case Opcode::GlobalAtomicMinF32x2:
    case Opcode::GlobalAtomicMaxF16x2:
    case Opcode::GlobalAtomicMaxF32x2:
    case Opcode::StorageAtomicIAdd32:
    case Opcode::StorageAtomicSMin32:
    case Opcode::StorageAtomicUMin32:
    case Opcode::StorageAtomicSMax32:
    case Opcode::StorageAtomicUMax32:
    case Opcode::StorageAtomicInc32:
    case Opcode::StorageAtomicDec32:
    case Opcode::StorageAtomicAnd32:
    case Opcode::StorageAtomicOr32:
    case Opcode::StorageAtomicXor32:
    case Opcode::StorageAtomicExchange32:
    case Opcode::StorageAtomicIAdd64:
    case Opcode::StorageAtomicSMin64:
    case Opcode::StorageAtomicUMin64:
    case Opcode::StorageAtomicSMax64:
    case Opcode::StorageAtomicUMax64:
    case Opcode::StorageAtomicAnd64:
    case Opcode::StorageAtomicOr64:
    case Opcode::StorageAtomicXor64:
    case Opcode::StorageAtomicExchange64:
    case Opcode::StorageAtomicAddF32:
    case Opcode::StorageAtomicAddF16x2:
    case Opcode::StorageAtomicAddF32x2:
    case Opcode::StorageAtomicMinF16x2:
    case Opcode::StorageAtomicMinF32x2:
    case Opcode::StorageAtomicMaxF16x2:
    case Opcode::StorageAtomicMaxF32x2:
    case Opcode::BindlessImageWrite:
    case Opcode::BoundImageWrite:
    case Opcode::ImageWrite:
    case IR::Opcode::BindlessImageAtomicIAdd32:
    case IR::Opcode::BindlessImageAtomicSMin32:
    case IR::Opcode::BindlessImageAtomicUMin32:
    case IR::Opcode::BindlessImageAtomicSMax32:
    case IR::Opcode::BindlessImageAtomicUMax32:
    case IR::Opcode::BindlessImageAtomicInc32:
    case IR::Opcode::BindlessImageAtomicDec32:
    case IR::Opcode::BindlessImageAtomicAnd32:
    case IR::Opcode::BindlessImageAtomicOr32:
    case IR::Opcode::BindlessImageAtomicXor32:
    case IR::Opcode::BindlessImageAtomicExchange32:
    case IR::Opcode::BoundImageAtomicIAdd32:
    case IR::Opcode::BoundImageAtomicSMin32:
    case IR::Opcode::BoundImageAtomicUMin32:
    case IR::Opcode::BoundImageAtomicSMax32:
    case IR::Opcode::BoundImageAtomicUMax32:
    case IR::Opcode::BoundImageAtomicInc32:
    case IR::Opcode::BoundImageAtomicDec32:
    case IR::Opcode::BoundImageAtomicAnd32:
    case IR::Opcode::BoundImageAtomicOr32:
    case IR::Opcode::BoundImageAtomicXor32:
    case IR::Opcode::BoundImageAtomicExchange32:
    case IR::Opcode::ImageAtomicIAdd32:
    case IR::Opcode::ImageAtomicSMin32:
    case IR::Opcode::ImageAtomicUMin32:
    case IR::Opcode::ImageAtomicSMax32:
    case IR::Opcode::ImageAtomicUMax32:
    case IR::Opcode::ImageAtomicInc32:
    case IR::Opcode::ImageAtomicDec32:
    case IR::Opcode::ImageAtomicAnd32:
    case IR::Opcode::ImageAtomicOr32:
    case IR::Opcode::ImageAtomicXor32:
    case IR::Opcode::ImageAtomicExchange32:
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
    case Opcode::GetSparseFromOp:
    case Opcode::GetInBoundsFromOp:
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

Inst* Inst::GetAssociatedPseudoOperation(IR::Opcode opcode) {
    if (!associated_insts) {
        return nullptr;
    }
    switch (opcode) {
    case Opcode::GetZeroFromOp:
        CheckPseudoInstruction(associated_insts->zero_inst, Opcode::GetZeroFromOp);
        return associated_insts->zero_inst;
    case Opcode::GetSignFromOp:
        CheckPseudoInstruction(associated_insts->sign_inst, Opcode::GetSignFromOp);
        return associated_insts->sign_inst;
    case Opcode::GetCarryFromOp:
        CheckPseudoInstruction(associated_insts->carry_inst, Opcode::GetCarryFromOp);
        return associated_insts->carry_inst;
    case Opcode::GetOverflowFromOp:
        CheckPseudoInstruction(associated_insts->overflow_inst, Opcode::GetOverflowFromOp);
        return associated_insts->overflow_inst;
    case Opcode::GetSparseFromOp:
        CheckPseudoInstruction(associated_insts->sparse_inst, Opcode::GetSparseFromOp);
        return associated_insts->sparse_inst;
    case Opcode::GetInBoundsFromOp:
        CheckPseudoInstruction(associated_insts->in_bounds_inst, Opcode::GetInBoundsFromOp);
        return associated_insts->in_bounds_inst;
    default:
        throw InvalidArgument("{} is not a pseudo-instruction", opcode);
    }
}

IR::Type Inst::Type() const {
    return TypeOf(op);
}

void Inst::SetArg(size_t index, Value value) {
    if (index >= NumArgs()) {
        throw InvalidArgument("Out of bounds argument index {} in opcode {}", index, op);
    }
    const IR::Value arg{Arg(index)};
    if (!arg.IsImmediate()) {
        UndoUse(arg);
    }
    if (!value.IsImmediate()) {
        Use(value);
    }
    if (op == Opcode::Phi) {
        phi_args[index].second = value;
    } else {
        args[index] = value;
    }
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
    ReplaceOpcode(Opcode::Void);
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
        }
        // Reset arguments to null
        // std::memset was measured to be faster on MSVC than std::ranges:fill
        std::memset(reinterpret_cast<char*>(&args), 0, sizeof(args));
    }
}

void Inst::ReplaceUsesWith(Value replacement) {
    Invalidate();
    ReplaceOpcode(Opcode::Identity);
    if (!replacement.IsImmediate()) {
        Use(replacement);
    }
    args[0] = replacement;
}

void Inst::ReplaceOpcode(IR::Opcode opcode) {
    if (opcode == IR::Opcode::Phi) {
        throw LogicError("Cannot transition into Phi");
    }
    if (op == Opcode::Phi) {
        // Transition out of phi arguments into non-phi
        std::destroy_at(&phi_args);
        std::construct_at(&args);
    }
    op = opcode;
}

void Inst::Use(const Value& value) {
    Inst* const inst{value.Inst()};
    ++inst->use_count;

    std::unique_ptr<AssociatedInsts>& assoc_inst{inst->associated_insts};
    switch (op) {
    case Opcode::GetZeroFromOp:
        AllocAssociatedInsts(assoc_inst);
        SetPseudoInstruction(assoc_inst->zero_inst, this);
        break;
    case Opcode::GetSignFromOp:
        AllocAssociatedInsts(assoc_inst);
        SetPseudoInstruction(assoc_inst->sign_inst, this);
        break;
    case Opcode::GetCarryFromOp:
        AllocAssociatedInsts(assoc_inst);
        SetPseudoInstruction(assoc_inst->carry_inst, this);
        break;
    case Opcode::GetOverflowFromOp:
        AllocAssociatedInsts(assoc_inst);
        SetPseudoInstruction(assoc_inst->overflow_inst, this);
        break;
    case Opcode::GetSparseFromOp:
        AllocAssociatedInsts(assoc_inst);
        SetPseudoInstruction(assoc_inst->sparse_inst, this);
        break;
    case Opcode::GetInBoundsFromOp:
        AllocAssociatedInsts(assoc_inst);
        SetPseudoInstruction(assoc_inst->in_bounds_inst, this);
        break;
    default:
        break;
    }
}

void Inst::UndoUse(const Value& value) {
    Inst* const inst{value.Inst()};
    --inst->use_count;

    std::unique_ptr<AssociatedInsts>& assoc_inst{inst->associated_insts};
    switch (op) {
    case Opcode::GetZeroFromOp:
        AllocAssociatedInsts(assoc_inst);
        RemovePseudoInstruction(assoc_inst->zero_inst, Opcode::GetZeroFromOp);
        break;
    case Opcode::GetSignFromOp:
        AllocAssociatedInsts(assoc_inst);
        RemovePseudoInstruction(assoc_inst->sign_inst, Opcode::GetSignFromOp);
        break;
    case Opcode::GetCarryFromOp:
        AllocAssociatedInsts(assoc_inst);
        RemovePseudoInstruction(assoc_inst->carry_inst, Opcode::GetCarryFromOp);
        break;
    case Opcode::GetOverflowFromOp:
        AllocAssociatedInsts(assoc_inst);
        RemovePseudoInstruction(assoc_inst->overflow_inst, Opcode::GetOverflowFromOp);
        break;
    case Opcode::GetSparseFromOp:
        AllocAssociatedInsts(assoc_inst);
        RemovePseudoInstruction(assoc_inst->sparse_inst, Opcode::GetSparseFromOp);
        break;
    case Opcode::GetInBoundsFromOp:
        AllocAssociatedInsts(assoc_inst);
        RemovePseudoInstruction(assoc_inst->in_bounds_inst, Opcode::GetInBoundsFromOp);
        break;
    default:
        break;
    }
}

} // namespace Shader::IR
