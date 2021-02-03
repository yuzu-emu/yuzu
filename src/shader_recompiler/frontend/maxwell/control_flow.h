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
    boost::container::small_vector<BlockId, 4> imm_predecessors;
    boost::container::small_vector<BlockId, 8> dominance_frontiers;
    union {
        bool post_order_visited{false};
        Block* imm_dominator;
    };
};

struct Label {
    Location address;
    BlockId block_id;
    Stack stack;
};

struct Function {
    Function(Location start_address);

    void BuildBlocksMap();

    void BuildImmediatePredecessors();

    void BuildPostOrder();

    void BuildImmediateDominators();

    void BuildDominanceFrontier();

    [[nodiscard]] size_t NumBlocks() const noexcept {
        return static_cast<size_t>(current_block_id) + 1;
    }

    Location entrypoint;
    BlockId current_block_id{0};
    boost::container::small_vector<Label, 16> labels;
    boost::container::small_vector<u32, 0x130> blocks;
    boost::container::small_vector<Block, 0x130> blocks_data;
    // Translates from BlockId to block index
    boost::container::small_vector<Block*, 0x130> blocks_map;

    boost::container::small_vector<u32, 0x130> post_order_blocks;
    boost::container::small_vector<BlockId, 0x130> post_order_map;
};

class CFG {
    enum class AnalysisState {
        Branch,
        Continue,
    };

public:
    explicit CFG(Environment& env, Location start_address);

    CFG& operator=(const CFG&) = delete;
    CFG(const CFG&) = delete;

    CFG& operator=(CFG&&) = delete;
    CFG(CFG&&) = delete;

    [[nodiscard]] std::string Dot() const;

    [[nodiscard]] std::span<const Function> Functions() const noexcept {
        return std::span(functions.data(), functions.size());
    }

private:
    void VisitFunctions(Location start_address);

    void AnalyzeLabel(FunctionId function_id, Label& label);

    /// Inspect already visited blocks.
    /// Return true when the block has already been visited
    bool InspectVisitedBlocks(FunctionId function_id, const Label& label);

    AnalysisState AnalyzeInst(Block& block, FunctionId function_id, Location pc);

    void AnalyzeCondInst(Block& block, FunctionId function_id, Location pc, EndClass insn_end_class,
                         IR::Condition cond);

    /// Return true when the branch instruction is confirmed to be a branch
    bool AnalyzeBranch(Block& block, FunctionId function_id, Location pc, Instruction inst,
                       Opcode opcode);

    void AnalyzeBRA(Block& block, FunctionId function_id, Location pc, Instruction inst,
                    bool is_absolute);
    void AnalyzeBRX(Block& block, Location pc, Instruction inst, bool is_absolute);
    void AnalyzeCAL(Location pc, Instruction inst, bool is_absolute);
    AnalysisState AnalyzeEXIT(Block& block, FunctionId function_id, Location pc, Instruction inst);

    /// Return the branch target block id
    BlockId AddLabel(const Block& block, Stack stack, Location pc, FunctionId function_id);

    Environment& env;
    boost::container::small_vector<Function, 1> functions;
    FunctionId current_function_id{0};
};

} // namespace Shader::Maxwell::Flow
