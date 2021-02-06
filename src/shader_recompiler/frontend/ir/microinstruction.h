// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstring>
#include <span>
#include <type_traits>
#include <vector>

#include <boost/intrusive/list.hpp>

#include "common/common_types.h"
#include "shader_recompiler/frontend/ir/opcodes.h"
#include "shader_recompiler/frontend/ir/type.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::IR {

class Block;

constexpr size_t MAX_ARG_COUNT = 4;

class Inst : public boost::intrusive::list_base_hook<> {
public:
    explicit Inst(Opcode op_, u64 flags_) noexcept : op{op_}, flags{flags_} {}

    /// Get the number of uses this instruction has.
    [[nodiscard]] int UseCount() const noexcept {
        return use_count;
    }

    /// Determines whether this instruction has uses or not.
    [[nodiscard]] bool HasUses() const noexcept {
        return use_count > 0;
    }

    /// Get the opcode this microinstruction represents.
    [[nodiscard]] IR::Opcode Opcode() const noexcept {
        return op;
    }

    /// Determines whether or not this instruction may have side effects.
    [[nodiscard]] bool MayHaveSideEffects() const noexcept;

    /// Determines whether or not this instruction is a pseudo-instruction.
    /// Pseudo-instructions depend on their parent instructions for their semantics.
    [[nodiscard]] bool IsPseudoInstruction() const noexcept;

    /// Determines if all arguments of this instruction are immediates.
    [[nodiscard]] bool AreAllArgsImmediates() const noexcept;

    /// Determines if there is a pseudo-operation associated with this instruction.
    [[nodiscard]] bool HasAssociatedPseudoOperation() const noexcept;
    /// Gets a pseudo-operation associated with this instruction
    [[nodiscard]] Inst* GetAssociatedPseudoOperation(IR::Opcode opcode);

    /// Get the number of arguments this instruction has.
    [[nodiscard]] size_t NumArgs() const;

    /// Get the type this instruction returns.
    [[nodiscard]] IR::Type Type() const;

    /// Get the value of a given argument index.
    [[nodiscard]] Value Arg(size_t index) const;
    /// Set the value of a given argument index.
    void SetArg(size_t index, Value value);

    /// Get an immutable span to the phi operands.
    [[nodiscard]] std::span<const std::pair<Block*, Value>> PhiOperands() const noexcept;
    /// Add phi operand to a phi instruction.
    void AddPhiOperand(Block* predecessor, const Value& value);

    void Invalidate();
    void ClearArgs();

    void ReplaceUsesWith(Value replacement);

    template <typename FlagsType>
    requires(sizeof(FlagsType) <= sizeof(u64) && std::is_trivially_copyable_v<FlagsType>)
        [[nodiscard]] FlagsType Flags() const noexcept {
        FlagsType ret;
        std::memcpy(&ret, &flags, sizeof(ret));
        return ret;
    }

private:
    void Use(const Value& value);
    void UndoUse(const Value& value);

    IR::Opcode op{};
    int use_count{};
    std::array<Value, MAX_ARG_COUNT> args{};
    Inst* zero_inst{};
    Inst* sign_inst{};
    Inst* carry_inst{};
    Inst* overflow_inst{};
    std::vector<std::pair<Block*, Value>> phi_operands;
    u64 flags{};
};

} // namespace Shader::IR
