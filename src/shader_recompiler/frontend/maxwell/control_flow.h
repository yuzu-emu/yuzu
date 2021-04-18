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
#include <boost/intrusive/set.hpp>

#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/ir/condition.h"
#include "shader_recompiler/frontend/maxwell/instruction.h"
#include "shader_recompiler/frontend/maxwell/location.h"
#include "shader_recompiler/frontend/maxwell/opcodes.h"
#include "shader_recompiler/object_pool.h"

namespace Shader::Maxwell::Flow {

struct Block;

using FunctionId = size_t;

enum class EndClass {
    Branch,
    IndirectBranch,
    Call,
    Exit,
    Return,
    Kill,
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

struct IndirectBranch {
    Block* block;
    u32 address;
};

struct Block : boost::intrusive::set_base_hook<
                   // Normal link is ~2.5% faster compared to safe link
                   boost::intrusive::link_mode<boost::intrusive::normal_link>> {
    [[nodiscard]] bool Contains(Location pc) const noexcept;

    bool operator<(const Block& rhs) const noexcept {
        return begin < rhs.begin;
    }

    Location begin;
    Location end;
    EndClass end_class{};
    IR::Condition cond{};
    Stack stack;
    Block* branch_true{};
    Block* branch_false{};
    FunctionId function_call{};
    Block* return_block{};
    IR::Reg branch_reg{};
    s32 branch_offset{};
    std::vector<IndirectBranch> indirect_branches;
};

struct Label {
    Location address;
    Block* block;
    Stack stack;
};

struct Function {
    explicit Function(ObjectPool<Block>& block_pool, Location start_address);

    Location entrypoint;
    boost::container::small_vector<Label, 16> labels;
    boost::intrusive::set<Block> blocks;
};

class CFG {
    enum class AnalysisState {
        Branch,
        Continue,
    };

public:
    explicit CFG(Environment& env, ObjectPool<Block>& block_pool, Location start_address,
                 bool exits_to_dispatcher = false);

    CFG& operator=(const CFG&) = delete;
    CFG(const CFG&) = delete;

    CFG& operator=(CFG&&) = delete;
    CFG(CFG&&) = delete;

    [[nodiscard]] std::string Dot() const;

    [[nodiscard]] std::span<const Function> Functions() const noexcept {
        return std::span(functions.data(), functions.size());
    }
    [[nodiscard]] std::span<Function> Functions() noexcept {
        return std::span(functions.data(), functions.size());
    }

    [[nodiscard]] bool ExitsToDispatcher() const {
        return exits_to_dispatcher;
    }

private:
    void AnalyzeLabel(FunctionId function_id, Label& label);

    /// Inspect already visited blocks.
    /// Return true when the block has already been visited
    bool InspectVisitedBlocks(FunctionId function_id, const Label& label);

    AnalysisState AnalyzeInst(Block* block, FunctionId function_id, Location pc);

    void AnalyzeCondInst(Block* block, FunctionId function_id, Location pc, EndClass insn_end_class,
                         IR::Condition cond);

    /// Return true when the branch instruction is confirmed to be a branch
    bool AnalyzeBranch(Block* block, FunctionId function_id, Location pc, Instruction inst,
                       Opcode opcode);

    void AnalyzeBRA(Block* block, FunctionId function_id, Location pc, Instruction inst,
                    bool is_absolute);
    AnalysisState AnalyzeBRX(Block* block, Location pc, Instruction inst, bool is_absolute,
                             FunctionId function_id);
    AnalysisState AnalyzeEXIT(Block* block, FunctionId function_id, Location pc, Instruction inst);

    /// Return the branch target block id
    Block* AddLabel(Block* block, Stack stack, Location pc, FunctionId function_id);

    Environment& env;
    ObjectPool<Block>& block_pool;
    boost::container::small_vector<Function, 1> functions;
    FunctionId current_function_id{0};
    Location program_start;
    bool exits_to_dispatcher{};
    Block* dispatch_block{};
};

} // namespace Shader::Maxwell::Flow
