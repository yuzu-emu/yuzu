// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstring>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/intrusive/list.hpp>

#include "common/bit_cast.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/ir/opcodes.h"
#include "shader_recompiler/frontend/ir/type.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::IR {

class Block;

constexpr size_t MAX_ARG_COUNT = 4;

class Inst : public boost::intrusive::list_base_hook<> {
public:
    explicit Inst(Opcode op_, u32 flags_) noexcept;
    ~Inst();

    Inst& operator=(const Inst&) = delete;
    Inst(const Inst&) = delete;

    Inst& operator=(Inst&&) = delete;
    Inst(Inst&&) = delete;

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
    [[nodiscard]] bool AreAllArgsImmediates() const;

    /// Determines if there is a pseudo-operation associated with this instruction.
    [[nodiscard]] bool HasAssociatedPseudoOperation() const noexcept;
    /// Gets a pseudo-operation associated with this instruction
    [[nodiscard]] Inst* GetAssociatedPseudoOperation(IR::Opcode opcode);

    /// Get the type this instruction returns.
    [[nodiscard]] IR::Type Type() const;

    /// Get the number of arguments this instruction has.
    [[nodiscard]] size_t NumArgs() const;

    /// Get the value of a given argument index.
    [[nodiscard]] Value Arg(size_t index) const;
    /// Set the value of a given argument index.
    void SetArg(size_t index, Value value);

    /// Get a pointer to the block of a phi argument.
    [[nodiscard]] Block* PhiBlock(size_t index) const;
    /// Add phi operand to a phi instruction.
    void AddPhiOperand(Block* predecessor, const Value& value);

    void Invalidate();
    void ClearArgs();

    void ReplaceUsesWith(Value replacement);

    void ReplaceOpcode(IR::Opcode opcode);

    template <typename FlagsType>
    requires(sizeof(FlagsType) <= sizeof(u32) && std::is_trivially_copyable_v<FlagsType>)
        [[nodiscard]] FlagsType Flags() const noexcept {
        FlagsType ret;
        std::memcpy(&ret, &flags, sizeof(ret));
        return ret;
    }

    /// Intrusively store the host definition of this instruction.
    template <typename DefinitionType>
    void SetDefinition(DefinitionType def) {
        definition = Common::BitCast<u32>(def);
    }

    /// Return the intrusively stored host definition of this instruction.
    template <typename DefinitionType>
    [[nodiscard]] DefinitionType Definition() const noexcept {
        return Common::BitCast<DefinitionType>(definition);
    }

private:
    struct NonTriviallyDummy {
        NonTriviallyDummy() noexcept {}
    };

    void Use(const Value& value);
    void UndoUse(const Value& value);

    IR::Opcode op{};
    int use_count{};
    u32 flags{};
    u32 definition{};
    union {
        NonTriviallyDummy dummy{};
        std::array<Value, MAX_ARG_COUNT> args;
        std::vector<std::pair<Block*, Value>> phi_args;
    };
    Inst* zero_inst{};
    Inst* sign_inst{};
    Inst* carry_inst{};
    Inst* overflow_inst{};
};
static_assert(sizeof(Inst) <= 128, "Inst size unintentionally increased its size");

} // namespace Shader::IR
