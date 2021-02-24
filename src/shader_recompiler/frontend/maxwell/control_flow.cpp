// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <optional>
#include <ranges>
#include <string>
#include <utility>

#include <fmt/format.h>

#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/maxwell/control_flow.h"
#include "shader_recompiler/frontend/maxwell/decode.h"
#include "shader_recompiler/frontend/maxwell/location.h"

namespace Shader::Maxwell::Flow {
namespace {
struct Compare {
    bool operator()(const Block& lhs, Location rhs) const noexcept {
        return lhs.begin < rhs;
    }

    bool operator()(Location lhs, const Block& rhs) const noexcept {
        return lhs < rhs.begin;
    }

    bool operator()(const Block& lhs, const Block& rhs) const noexcept {
        return lhs.begin < rhs.begin;
    }
};
} // Anonymous namespace

static u32 BranchOffset(Location pc, Instruction inst) {
    return pc.Offset() + inst.branch.Offset() + 8;
}

static void Split(Block* old_block, Block* new_block, Location pc) {
    if (pc <= old_block->begin || pc >= old_block->end) {
        throw InvalidArgument("Invalid address to split={}", pc);
    }
    *new_block = Block{
        .begin{pc},
        .end{old_block->end},
        .end_class{old_block->end_class},
        .stack{old_block->stack},
        .cond{old_block->cond},
        .branch_true{old_block->branch_true},
        .branch_false{old_block->branch_false},
        .ir{nullptr},
    };
    *old_block = Block{
        .begin{old_block->begin},
        .end{pc},
        .end_class{EndClass::Branch},
        .stack{std::move(old_block->stack)},
        .cond{IR::Condition{true}},
        .branch_true{new_block},
        .branch_false{nullptr},
        .ir{nullptr},
    };
}

static Token OpcodeToken(Opcode opcode) {
    switch (opcode) {
    case Opcode::PBK:
    case Opcode::BRK:
        return Token::PBK;
    case Opcode::PCNT:
    case Opcode::CONT:
        return Token::PBK;
    case Opcode::PEXIT:
    case Opcode::EXIT:
        return Token::PEXIT;
    case Opcode::PLONGJMP:
    case Opcode::LONGJMP:
        return Token::PLONGJMP;
    case Opcode::PRET:
    case Opcode::RET:
    case Opcode::CAL:
        return Token::PRET;
    case Opcode::SSY:
    case Opcode::SYNC:
        return Token::SSY;
    default:
        throw InvalidArgument("{}", opcode);
    }
}

static bool IsAbsoluteJump(Opcode opcode) {
    switch (opcode) {
    case Opcode::JCAL:
    case Opcode::JMP:
    case Opcode::JMX:
        return true;
    default:
        return false;
    }
}

static bool HasFlowTest(Opcode opcode) {
    switch (opcode) {
    case Opcode::BRA:
    case Opcode::BRX:
    case Opcode::EXIT:
    case Opcode::JMP:
    case Opcode::JMX:
    case Opcode::BRK:
    case Opcode::CONT:
    case Opcode::LONGJMP:
    case Opcode::RET:
    case Opcode::SYNC:
        return true;
    case Opcode::CAL:
    case Opcode::JCAL:
        return false;
    default:
        throw InvalidArgument("Invalid branch {}", opcode);
    }
}

static std::string NameOf(const Block& block) {
    if (block.begin.IsVirtual()) {
        return fmt::format("\"Virtual {}\"", block.begin);
    } else {
        return fmt::format("\"{}\"", block.begin);
    }
}

void Stack::Push(Token token, Location target) {
    entries.push_back({
        .token{token},
        .target{target},
    });
}

std::pair<Location, Stack> Stack::Pop(Token token) const {
    const std::optional<Location> pc{Peek(token)};
    if (!pc) {
        throw LogicError("Token could not be found");
    }
    return {*pc, Remove(token)};
}

std::optional<Location> Stack::Peek(Token token) const {
    const auto reverse_entries{entries | std::views::reverse};
    const auto it{std::ranges::find(reverse_entries, token, &StackEntry::token)};
    if (it == reverse_entries.end()) {
        return std::nullopt;
    }
    return it->target;
}

Stack Stack::Remove(Token token) const {
    const auto reverse_entries{entries | std::views::reverse};
    const auto it{std::ranges::find(reverse_entries, token, &StackEntry::token)};
    const auto pos{std::distance(reverse_entries.begin(), it)};
    Stack result;
    result.entries.insert(result.entries.end(), entries.begin(), entries.end() - pos - 1);
    return result;
}

bool Block::Contains(Location pc) const noexcept {
    return pc >= begin && pc < end;
}

Function::Function(Location start_address)
    : entrypoint{start_address}, labels{{
                                     .address{start_address},
                                     .block{nullptr},
                                     .stack{},
                                 }} {}

CFG::CFG(Environment& env_, ObjectPool<Block>& block_pool_, Location start_address)
    : env{env_}, block_pool{block_pool_} {
    functions.emplace_back(start_address);
    functions.back().labels.back().block = block_pool.Create(Block{
        .begin{start_address},
        .end{start_address},
        .end_class{EndClass::Branch},
        .stack{},
        .cond{IR::Condition{true}},
        .branch_true{nullptr},
        .branch_false{nullptr},
        .ir{nullptr},
    });
    for (FunctionId function_id = 0; function_id < functions.size(); ++function_id) {
        while (!functions[function_id].labels.empty()) {
            Function& function{functions[function_id]};
            Label label{function.labels.back()};
            function.labels.pop_back();
            AnalyzeLabel(function_id, label);
        }
    }
}

void CFG::AnalyzeLabel(FunctionId function_id, Label& label) {
    if (InspectVisitedBlocks(function_id, label)) {
        // Label address has been visited
        return;
    }
    // Try to find the next block
    Function* const function{&functions[function_id]};
    Location pc{label.address};
    const auto next_it{function->blocks.upper_bound(pc, Compare{})};
    const bool is_last{next_it == function->blocks.end()};
    Block* const next{is_last ? nullptr : &*next_it};
    // Insert before the next block
    Block* const block{label.block};
    // Analyze instructions until it reaches an already visited block or there's a branch
    bool is_branch{false};
    while (!next || pc < next->begin) {
        is_branch = AnalyzeInst(block, function_id, pc) == AnalysisState::Branch;
        if (is_branch) {
            break;
        }
        ++pc;
    }
    if (!is_branch) {
        // If the block finished without a branch,
        // it means that the next instruction is already visited, jump to it
        block->end = pc;
        block->cond = IR::Condition{true};
        block->branch_true = next;
        block->branch_false = nullptr;
    }
    // Function's pointer might be invalid, resolve it again
    // Insert the new block
    functions[function_id].blocks.insert(*block);
}

bool CFG::InspectVisitedBlocks(FunctionId function_id, const Label& label) {
    const Location pc{label.address};
    Function& function{functions[function_id]};
    const auto it{
        std::ranges::find_if(function.blocks, [pc](auto& block) { return block.Contains(pc); })};
    if (it == function.blocks.end()) {
        // Address has not been visited
        return false;
    }
    Block* const visited_block{&*it};
    if (visited_block->begin == pc) {
        throw LogicError("Dangling block");
    }
    Block* const new_block{label.block};
    Split(visited_block, new_block, pc);
    function.blocks.insert(it, *new_block);
    return true;
}

CFG::AnalysisState CFG::AnalyzeInst(Block* block, FunctionId function_id, Location pc) {
    const Instruction inst{env.ReadInstruction(pc.Offset())};
    const Opcode opcode{Decode(inst.raw)};
    switch (opcode) {
    case Opcode::BRA:
    case Opcode::BRX:
    case Opcode::JMP:
    case Opcode::JMX:
    case Opcode::RET:
        if (!AnalyzeBranch(block, function_id, pc, inst, opcode)) {
            return AnalysisState::Continue;
        }
        switch (opcode) {
        case Opcode::BRA:
        case Opcode::JMP:
            AnalyzeBRA(block, function_id, pc, inst, IsAbsoluteJump(opcode));
            break;
        case Opcode::BRX:
        case Opcode::JMX:
            AnalyzeBRX(block, pc, inst, IsAbsoluteJump(opcode));
            break;
        case Opcode::RET:
            block->end_class = EndClass::Return;
            break;
        default:
            break;
        }
        block->end = pc;
        return AnalysisState::Branch;
    case Opcode::BRK:
    case Opcode::CONT:
    case Opcode::LONGJMP:
    case Opcode::SYNC: {
        if (!AnalyzeBranch(block, function_id, pc, inst, opcode)) {
            return AnalysisState::Continue;
        }
        const auto [stack_pc, new_stack]{block->stack.Pop(OpcodeToken(opcode))};
        block->branch_true = AddLabel(block, new_stack, stack_pc, function_id);
        block->end = pc;
        return AnalysisState::Branch;
    }
    case Opcode::PBK:
    case Opcode::PCNT:
    case Opcode::PEXIT:
    case Opcode::PLONGJMP:
    case Opcode::SSY:
        block->stack.Push(OpcodeToken(opcode), BranchOffset(pc, inst));
        return AnalysisState::Continue;
    case Opcode::EXIT:
        return AnalyzeEXIT(block, function_id, pc, inst);
    case Opcode::PRET:
        throw NotImplementedException("PRET flow analysis");
    case Opcode::CAL:
    case Opcode::JCAL: {
        const bool is_absolute{IsAbsoluteJump(opcode)};
        const Location cal_pc{is_absolute ? inst.branch.Absolute() : BranchOffset(pc, inst)};
        // Technically CAL pushes into PRET, but that's implicit in the function call for us
        // Insert the function into the list if it doesn't exist
        if (std::ranges::find(functions, cal_pc, &Function::entrypoint) == functions.end()) {
            functions.emplace_back(cal_pc);
        }
        // Handle CAL like a regular instruction
        break;
    }
    default:
        break;
    }
    const Predicate pred{inst.Pred()};
    if (pred == Predicate{true} || pred == Predicate{false}) {
        return AnalysisState::Continue;
    }
    const IR::Condition cond{static_cast<IR::Pred>(pred.index), pred.negated};
    AnalyzeCondInst(block, function_id, pc, EndClass::Branch, cond);
    return AnalysisState::Branch;
}

void CFG::AnalyzeCondInst(Block* block, FunctionId function_id, Location pc,
                          EndClass insn_end_class, IR::Condition cond) {
    if (block->begin != pc) {
        // If the block doesn't start in the conditional instruction
        // mark it as a label to visit it later
        block->end = pc;
        block->cond = IR::Condition{true};
        block->branch_true = AddLabel(block, block->stack, pc, function_id);
        block->branch_false = nullptr;
        return;
    }
    // Create a virtual block and a conditional block
    Block* const conditional_block{block_pool.Create()};
    Block virtual_block{
        .begin{block->begin.Virtual()},
        .end{block->begin.Virtual()},
        .end_class{EndClass::Branch},
        .stack{block->stack},
        .cond{cond},
        .branch_true{conditional_block},
        .branch_false{nullptr},
        .ir{nullptr},
    };
    // Save the contents of the visited block in the conditional block
    *conditional_block = std::move(*block);
    // Impersonate the visited block with a virtual block
    *block = std::move(virtual_block);
    // Set the end properties of the conditional instruction
    conditional_block->end = pc + 1;
    conditional_block->end_class = insn_end_class;
    // Add a label to the instruction after the conditional instruction
    Block* const endif_block{AddLabel(conditional_block, block->stack, pc + 1, function_id)};
    // Branch to the next instruction from the virtual block
    block->branch_false = endif_block;
    // And branch to it from the conditional instruction if it is a branch
    if (insn_end_class == EndClass::Branch) {
        conditional_block->cond = IR::Condition{true};
        conditional_block->branch_true = endif_block;
        conditional_block->branch_false = nullptr;
    }
    // Finally insert the condition block into the list of blocks
    functions[function_id].blocks.insert(*conditional_block);
}

bool CFG::AnalyzeBranch(Block* block, FunctionId function_id, Location pc, Instruction inst,
                        Opcode opcode) {
    if (inst.branch.is_cbuf) {
        throw NotImplementedException("Branch with constant buffer offset");
    }
    const Predicate pred{inst.Pred()};
    if (pred == Predicate{false}) {
        return false;
    }
    const bool has_flow_test{HasFlowTest(opcode)};
    const IR::FlowTest flow_test{has_flow_test ? inst.branch.flow_test.Value() : IR::FlowTest::T};
    if (pred != Predicate{true} || flow_test != IR::FlowTest::T) {
        block->cond = IR::Condition(flow_test, static_cast<IR::Pred>(pred.index), pred.negated);
        block->branch_false = AddLabel(block, block->stack, pc + 1, function_id);
    } else {
        block->cond = IR::Condition{true};
    }
    return true;
}

void CFG::AnalyzeBRA(Block* block, FunctionId function_id, Location pc, Instruction inst,
                     bool is_absolute) {
    const Location bra_pc{is_absolute ? inst.branch.Absolute() : BranchOffset(pc, inst)};
    block->branch_true = AddLabel(block, block->stack, bra_pc, function_id);
}

void CFG::AnalyzeBRX(Block*, Location, Instruction, bool is_absolute) {
    throw NotImplementedException("{}", is_absolute ? "JMX" : "BRX");
}

void CFG::AnalyzeCAL(Location pc, Instruction inst, bool is_absolute) {
    const Location cal_pc{is_absolute ? inst.branch.Absolute() : BranchOffset(pc, inst)};
    // Technically CAL pushes into PRET, but that's implicit in the function call for us
    // Insert the function to the function list if it doesn't exist
    const auto it{std::ranges::find(functions, cal_pc, &Function::entrypoint)};
    if (it == functions.end()) {
        functions.emplace_back(cal_pc);
    }
}

CFG::AnalysisState CFG::AnalyzeEXIT(Block* block, FunctionId function_id, Location pc,
                                    Instruction inst) {
    const IR::FlowTest flow_test{inst.branch.flow_test};
    const Predicate pred{inst.Pred()};
    if (pred == Predicate{false} || flow_test == IR::FlowTest::F) {
        // EXIT will never be taken
        return AnalysisState::Continue;
    }
    if (pred != Predicate{true} || flow_test != IR::FlowTest::T) {
        if (block->stack.Peek(Token::PEXIT).has_value()) {
            throw NotImplementedException("Conditional EXIT with PEXIT token");
        }
        const IR::Condition cond{flow_test, static_cast<IR::Pred>(pred.index), pred.negated};
        AnalyzeCondInst(block, function_id, pc, EndClass::Exit, cond);
        return AnalysisState::Branch;
    }
    if (const std::optional<Location> exit_pc{block->stack.Peek(Token::PEXIT)}) {
        const Stack popped_stack{block->stack.Remove(Token::PEXIT)};
        block->cond = IR::Condition{true};
        block->branch_true = AddLabel(block, popped_stack, *exit_pc, function_id);
        block->branch_false = nullptr;
        return AnalysisState::Branch;
    }
    block->end = pc;
    block->end_class = EndClass::Exit;
    return AnalysisState::Branch;
}

Block* CFG::AddLabel(Block* block, Stack stack, Location pc, FunctionId function_id) {
    Function& function{functions[function_id]};
    if (block->begin == pc) {
        // Jumps to itself
        return block;
    }
    if (const auto it{function.blocks.find(pc, Compare{})}; it != function.blocks.end()) {
        // Block already exists and it has been visited
        return &*it;
    }
    // TODO: FIX DANGLING BLOCKS
    Block* const new_block{block_pool.Create(Block{
        .begin{pc},
        .end{pc},
        .end_class{EndClass::Branch},
        .stack{stack},
        .cond{IR::Condition{true}},
        .branch_true{nullptr},
        .branch_false{nullptr},
        .ir{nullptr},
    })};
    function.labels.push_back(Label{
        .address{pc},
        .block{new_block},
        .stack{std::move(stack)},
    });
    return new_block;
}

std::string CFG::Dot() const {
    int node_uid{0};

    std::string dot{"digraph shader {\n"};
    for (const Function& function : functions) {
        dot += fmt::format("\tsubgraph cluster_{} {{\n", function.entrypoint);
        dot += fmt::format("\t\tnode [style=filled];\n");
        for (const Block& block : function.blocks) {
            const std::string name{NameOf(block)};
            const auto add_branch = [&](Block* branch, bool add_label) {
                dot += fmt::format("\t\t{}->{}", name, NameOf(*branch));
                if (add_label && block.cond != IR::Condition{true} &&
                    block.cond != IR::Condition{false}) {
                    dot += fmt::format(" [label=\"{}\"]", block.cond);
                }
                dot += '\n';
            };
            dot += fmt::format("\t\t{};\n", name);
            switch (block.end_class) {
            case EndClass::Branch:
                if (block.cond != IR::Condition{false}) {
                    add_branch(block.branch_true, true);
                }
                if (block.cond != IR::Condition{true}) {
                    add_branch(block.branch_false, false);
                }
                break;
            case EndClass::Exit:
                dot += fmt::format("\t\t{}->N{};\n", name, node_uid);
                dot += fmt::format("\t\tN{} [label=\"Exit\"][shape=square][style=stripped];\n",
                                   node_uid);
                ++node_uid;
                break;
            case EndClass::Return:
                dot += fmt::format("\t\t{}->N{};\n", name, node_uid);
                dot += fmt::format("\t\tN{} [label=\"Return\"][shape=square][style=stripped];\n",
                                   node_uid);
                ++node_uid;
                break;
            }
        }
        if (function.entrypoint == 8) {
            dot += fmt::format("\t\tlabel = \"main\";\n");
        } else {
            dot += fmt::format("\t\tlabel = \"Function {}\";\n", function.entrypoint);
        }
        dot += "\t}\n";
    }
    if (!functions.empty()) {
        auto& function{functions.front()};
        if (function.blocks.empty()) {
            dot += "Start;\n";
        } else {
            dot += fmt::format("\tStart -> {};\n", NameOf(*function.blocks.begin()));
        }
        dot += fmt::format("\tStart [shape=diamond];\n");
    }
    dot += "}\n";
    return dot;
}

} // namespace Shader::Maxwell::Flow
