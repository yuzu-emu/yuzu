// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstring>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/container/small_vector.hpp>
#include <boost/intrusive/list.hpp>

#include "common/bit_cast.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/ir/opcodes.h"
#include "shader_recompiler/frontend/ir/type.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::IR {

class Block;

struct AssociatedInsts;

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
    [[nodiscard]] IR::Opcode GetOpcode() const noexcept {
        return op;
    }

    /// Determines if there is a pseudo-operation associated with this instruction.
    [[nodiscard]] bool HasAssociatedPseudoOperation() const noexcept {
        return associated_insts != nullptr;
    }

    /// Determines whether or not this instruction may have side effects.
    [[nodiscard]] bool MayHaveSideEffects() const noexcept;

    /// Determines whether or not this instruction is a pseudo-instruction.
    /// Pseudo-instructions depend on their parent instructions for their semantics.
    [[nodiscard]] bool IsPseudoInstruction() const noexcept;

    /// Determines if all arguments of this instruction are immediates.
    [[nodiscard]] bool AreAllArgsImmediates() const;

    /// Gets a pseudo-operation associated with this instruction
    [[nodiscard]] Inst* GetAssociatedPseudoOperation(IR::Opcode opcode);

    /// Get the type this instruction returns.
    [[nodiscard]] IR::Type Type() const;

    /// Get the number of arguments this instruction has.
    [[nodiscard]] size_t NumArgs() const {
        return op == Opcode::Phi ? phi_args.size() : NumArgsOf(op);
    }

    /// Get the value of a given argument index.
    [[nodiscard]] Value Arg(size_t index) const noexcept {
        if (op == Opcode::Phi) {
            return phi_args[index].second;
        } else {
            return args[index];
        }
    }

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
        std::memcpy(reinterpret_cast<char*>(&ret), &flags, sizeof(ret));
        return ret;
    }

    template <typename FlagsType>
    requires(sizeof(FlagsType) <= sizeof(u32) && std::is_trivially_copyable_v<FlagsType>)
        [[nodiscard]] void SetFlags(FlagsType value) noexcept {
        std::memcpy(&flags, &value, sizeof(value));
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
        boost::container::small_vector<std::pair<Block*, Value>, 2> phi_args;
        std::array<Value, 5> args;
    };
    std::unique_ptr<AssociatedInsts> associated_insts;
};
static_assert(sizeof(Inst) <= 128, "Inst size unintentionally increased");

struct AssociatedInsts {
    union {
        Inst* in_bounds_inst;
        Inst* sparse_inst;
        Inst* zero_inst{};
    };
    Inst* sign_inst{};
    Inst* carry_inst{};
    Inst* overflow_inst{};
};

} // namespace Shader::IR
