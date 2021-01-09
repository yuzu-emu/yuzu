// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <compare>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <boost/container/small_vector.hpp>

#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/ir/condition.h"
#include "shader_recompiler/frontend/maxwell/instruction.h"
#include "shader_recompiler/frontend/maxwell/location.h"
#include "shader_recompiler/frontend/maxwell/opcode.h"

namespace Shader::Maxwell::Flow {

using BlockId = u32;
using FunctionId = size_t;

constexpr BlockId UNREACHABLE_BLOCK_ID{static_cast<u32>(-1)};

enum class EndClass {
    Branch,
    Exit,
    Return,
    Unreachable,
};

enum class Token {
    SSY,
    PBK,
    PEXIT,
    PRET,
    PCNT,
    PLONGJMP,
};

struct StackEntry {
    auto operator<=>(const StackEntry&) const noexcept = default;

    Token token;
    Location target;
};

class Stack {
public:
    void Push(Token token, Location target);
    [[nodiscard]] std::pair<Location, Stack> Pop(Token token) const;
    [[nodiscard]] std::optional<Location> Peek(Token token) const;
    [[nodiscard]] Stack Remove(Token token) const;

private:
    boost::container::small_vector<StackEntry, 3> entries;
};

struct Block {
    [[nodiscard]] bool Contains(Location pc) const noexcept;

    Location begin;
    Location end;
    EndClass end_class;
    BlockId id;
    Stack stack;
    IR::Condition cond;
    BlockId branch_true;
    BlockId branch_false;
};

struct Label {
    Location address;
    BlockId block_id;
    Stack stack;
};

struct Function {
    Function(Location start_address);

    Location entrypoint;
    BlockId current_block_id{0};
    boost::container::small_vector<Label, 16> labels;
    boost::container::small_vector<u32, 0x130> blocks;
    boost::container::small_vector<Block, 0x130> blocks_data;
};

class CFG {
    enum class AnalysisState {
        Branch,
        Continue,
    };

public:
    explicit CFG(Environment& env, Location start_address);

    [[nodiscard]] std::string Dot() const;

    [[nodiscard]] std::span<const Function> Functions() const noexcept {
        return std::span(functions.data(), functions.size());
    }

private:
    void AnalyzeLabel(FunctionId function_id, Label& label);

    /// Inspect already visited blocks.
    /// Return true when the block has already been visited
    [[nodiscard]] bool InspectVisitedBlocks(FunctionId function_id, const Label& label);

    [[nodiscard]] AnalysisState AnalyzeInst(Block& block, FunctionId function_id, Location pc);

    void AnalyzeCondInst(Block& block, FunctionId function_id, Location pc, EndClass insn_end_class,
                         IR::Condition cond);

    /// Return true when the branch instruction is confirmed to be a branch
    [[nodiscard]] bool AnalyzeBranch(Block& block, FunctionId function_id, Location pc,
                                     Instruction inst, Opcode opcode);

    void AnalyzeBRA(Block& block, FunctionId function_id, Location pc, Instruction inst,
                    bool is_absolute);
    void AnalyzeBRX(Block& block, Location pc, Instruction inst, bool is_absolute);
    void AnalyzeCAL(Location pc, Instruction inst, bool is_absolute);
    AnalysisState AnalyzeEXIT(Block& block, FunctionId function_id, Location pc, Instruction inst);

    /// Return the branch target block id
    [[nodiscard]] BlockId AddLabel(const Block& block, Stack stack, Location pc,
                                   FunctionId function_id);

    Environment& env;
    boost::container::small_vector<Function, 1> functions;
    FunctionId current_function_id{0};
};

} // namespace Shader::Maxwell::Flow
