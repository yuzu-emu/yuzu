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

static u32 BranchOffset(Location pc, Instruction inst) {
    return pc.Offset() + inst.branch.Offset() + 8;
}

static std::array<Block, 2> Split(Block&& block, Location pc, BlockId new_id) {
    if (pc <= block.begin || pc >= block.end) {
        throw InvalidArgument("Invalid address to split={}", pc);
    }
    return {
        Block{
            .begin{block.begin},
            .end{pc},
            .end_class{EndClass::Branch},
            .id{block.id},
            .stack{block.stack},
            .cond{true},
            .branch_true{new_id},
            .branch_false{UNREACHABLE_BLOCK_ID},
            .imm_predecessors{},
        },
        Block{
            .begin{pc},
            .end{block.end},
            .end_class{block.end_class},
            .id{new_id},
            .stack{std::move(block.stack)},
            .cond{block.cond},
            .branch_true{block.branch_true},
            .branch_false{block.branch_false},
            .imm_predecessors{},
        },
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
        return fmt::format("\"Virtual {}\"", block.id);
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
                                     .block_id{0},
                                     .stack{},
                                 }} {}

void Function::BuildBlocksMap() {
    const size_t num_blocks{NumBlocks()};
    blocks_map.resize(num_blocks);
    for (size_t block_index = 0; block_index < num_blocks; ++block_index) {
        Block& block{blocks_data[block_index]};
        blocks_map[block.id] = &block;
    }
}

void Function::BuildImmediatePredecessors() {
    for (const Block& block : blocks_data) {
        if (block.branch_true != UNREACHABLE_BLOCK_ID) {
            blocks_map[block.branch_true]->imm_predecessors.push_back(block.id);
        }
        if (block.branch_false != UNREACHABLE_BLOCK_ID) {
            blocks_map[block.branch_false]->imm_predecessors.push_back(block.id);
        }
    }
}

void Function::BuildPostOrder() {
    boost::container::small_vector<BlockId, 0x110> block_stack;
    post_order_map.resize(NumBlocks());

    Block& first_block{blocks_data[blocks.front()]};
    first_block.post_order_visited = true;
    block_stack.push_back(first_block.id);

    const auto visit_branch = [&](BlockId block_id, BlockId branch_id) {
        if (branch_id == UNREACHABLE_BLOCK_ID) {
            return false;
        }
        if (blocks_map[branch_id]->post_order_visited) {
            return false;
        }
        blocks_map[branch_id]->post_order_visited = true;

        // Calling push_back twice is faster than insert on msvc
        block_stack.push_back(block_id);
        block_stack.push_back(branch_id);
        return true;
    };
    while (!block_stack.empty()) {
        const Block* const block{blocks_map[block_stack.back()]};
        block_stack.pop_back();

        if (!visit_branch(block->id, block->branch_true) &&
            !visit_branch(block->id, block->branch_false)) {
            post_order_map[block->id] = static_cast<u32>(post_order_blocks.size());
            post_order_blocks.push_back(block->id);
        }
    }
}

void Function::BuildImmediateDominators() {
    auto transform_block_id{std::views::transform([this](BlockId id) { return blocks_map[id]; })};
    auto reverse_order_but_first{std::views::reverse | std::views::drop(1) | transform_block_id};
    auto has_idom{std::views::filter([](Block* block) { return block->imm_dominator; })};
    auto intersect{[this](Block* finger1, Block* finger2) {
        while (finger1 != finger2) {
            while (post_order_map[finger1->id] < post_order_map[finger2->id]) {
                finger1 = finger1->imm_dominator;
            }
            while (post_order_map[finger2->id] < post_order_map[finger1->id]) {
                finger2 = finger2->imm_dominator;
            }
        }
        return finger1;
    }};
    for (Block& block : blocks_data) {
        block.imm_dominator = nullptr;
    }
    Block* const start_block{&blocks_data[blocks.front()]};
    start_block->imm_dominator = start_block;

    bool changed{true};
    while (changed) {
        changed = false;
        for (Block* const block : post_order_blocks | reverse_order_but_first) {
            Block* new_idom{};
            for (Block* predecessor : block->imm_predecessors | transform_block_id | has_idom) {
                new_idom = new_idom ? intersect(predecessor, new_idom) : predecessor;
            }
            changed |= block->imm_dominator != new_idom;
            block->imm_dominator = new_idom;
        }
    }
}

void Function::BuildDominanceFrontier() {
    auto transform_block_id{std::views::transform([this](BlockId id) { return blocks_map[id]; })};
    auto has_enough_predecessors{[](Block& block) { return block.imm_predecessors.size() >= 2; }};
    for (Block& block : blocks_data | std::views::filter(has_enough_predecessors)) {
        for (Block* current : block.imm_predecessors | transform_block_id) {
            while (current != block.imm_dominator) {
                current->dominance_frontiers.push_back(current->id);
                current = current->imm_dominator;
            }
        }
    }
}

CFG::CFG(Environment& env_, Location start_address) : env{env_} {
    VisitFunctions(start_address);

    for (Function& function : functions) {
        function.BuildBlocksMap();
        function.BuildImmediatePredecessors();
        function.BuildPostOrder();
        function.BuildImmediateDominators();
        function.BuildDominanceFrontier();
    }
}

void CFG::VisitFunctions(Location start_address) {
    functions.emplace_back(start_address);
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
    Function* function{&functions[function_id]};
    Location pc{label.address};
    const auto next{std::upper_bound(function->blocks.begin(), function->blocks.end(), pc,
                                     [function](Location pc, u32 block_index) {
                                         return pc < function->blocks_data[block_index].begin;
                                     })};
    const auto next_index{std::distance(function->blocks.begin(), next)};
    const bool is_last{next == function->blocks.end()};
    Location next_pc;
    BlockId next_id{UNREACHABLE_BLOCK_ID};
    if (!is_last) {
        next_pc = function->blocks_data[*next].begin;
        next_id = function->blocks_data[*next].id;
    }
    // Insert before the next block
    Block block{
        .begin{pc},
        .end{pc},
        .end_class{EndClass::Branch},
        .id{label.block_id},
        .stack{std::move(label.stack)},
        .cond{true},
        .branch_true{UNREACHABLE_BLOCK_ID},
        .branch_false{UNREACHABLE_BLOCK_ID},
        .imm_predecessors{},
    };
    // Analyze instructions until it reaches an already visited block or there's a branch
    bool is_branch{false};
    while (is_last || pc < next_pc) {
        is_branch = AnalyzeInst(block, function_id, pc) == AnalysisState::Branch;
        if (is_branch) {
            break;
        }
        ++pc;
    }
    if (!is_branch) {
        // If the block finished without a branch,
        // it means that the next instruction is already visited, jump to it
        block.end = pc;
        block.cond = true;
        block.branch_true = next_id;
        block.branch_false = UNREACHABLE_BLOCK_ID;
    }
    // Function's pointer might be invalid, resolve it again
    function = &functions[function_id];
    const u32 new_block_index = static_cast<u32>(function->blocks_data.size());
    function->blocks.insert(function->blocks.begin() + next_index, new_block_index);
    function->blocks_data.push_back(std::move(block));
}

bool CFG::InspectVisitedBlocks(FunctionId function_id, const Label& label) {
    const Location pc{label.address};
    Function& function{functions[function_id]};
    const auto it{std::ranges::find_if(function.blocks, [&function, pc](u32 block_index) {
        return function.blocks_data[block_index].Contains(pc);
    })};
    if (it == function.blocks.end()) {
        // Address has not been visited
        return false;
    }
    Block& block{function.blocks_data[*it]};
    if (block.begin == pc) {
        throw LogicError("Dangling branch");
    }
    const u32 first_index{*it};
    const u32 second_index{static_cast<u32>(function.blocks_data.size())};
    const std::array new_indices{first_index, second_index};
    std::array split_blocks{Split(std::move(block), pc, label.block_id)};
    function.blocks_data[*it] = std::move(split_blocks[0]);
    function.blocks_data.push_back(std::move(split_blocks[1]));
    function.blocks.insert(function.blocks.erase(it), new_indices.begin(), new_indices.end());
    return true;
}

CFG::AnalysisState CFG::AnalyzeInst(Block& block, FunctionId function_id, Location pc) {
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
            block.end_class = EndClass::Return;
            break;
        default:
            break;
        }
        block.end = pc;
        return AnalysisState::Branch;
    case Opcode::BRK:
    case Opcode::CONT:
    case Opcode::LONGJMP:
    case Opcode::SYNC: {
        if (!AnalyzeBranch(block, function_id, pc, inst, opcode)) {
            return AnalysisState::Continue;
        }
        const auto [stack_pc, new_stack]{block.stack.Pop(OpcodeToken(opcode))};
        block.branch_true = AddLabel(block, new_stack, stack_pc, function_id);
        block.end = pc;
        return AnalysisState::Branch;
    }
    case Opcode::PBK:
    case Opcode::PCNT:
    case Opcode::PEXIT:
    case Opcode::PLONGJMP:
    case Opcode::SSY:
        block.stack.Push(OpcodeToken(opcode), BranchOffset(pc, inst));
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

void CFG::AnalyzeCondInst(Block& block, FunctionId function_id, Location pc,
                          EndClass insn_end_class, IR::Condition cond) {
    if (block.begin != pc) {
        // If the block doesn't start in the conditional instruction
        // mark it as a label to visit it later
        block.end = pc;
        block.cond = true;
        block.branch_true = AddLabel(block, block.stack, pc, function_id);
        block.branch_false = UNREACHABLE_BLOCK_ID;
        return;
    }
    // Impersonate the visited block with a virtual block
    // Jump from this virtual to the real conditional instruction and the next instruction
    Function& function{functions[function_id]};
    const BlockId conditional_block_id{++function.current_block_id};
    function.blocks.push_back(static_cast<u32>(function.blocks_data.size()));
    Block& virtual_block{function.blocks_data.emplace_back(Block{
        .begin{}, // Virtual block
        .end{},
        .end_class{EndClass::Branch},
        .id{block.id}, // Impersonating
        .stack{block.stack},
        .cond{cond},
        .branch_true{conditional_block_id},
        .branch_false{UNREACHABLE_BLOCK_ID},
        .imm_predecessors{},
    })};
    // Set the end properties of the conditional instruction and give it a new identity
    Block& conditional_block{block};
    conditional_block.end = pc;
    conditional_block.end_class = insn_end_class;
    conditional_block.id = conditional_block_id;
    // Add a label to the instruction after the conditional instruction
    const BlockId endif_block_id{AddLabel(conditional_block, block.stack, pc + 1, function_id)};
    // Branch to the next instruction from the virtual block
    virtual_block.branch_false = endif_block_id;
    // And branch to it from the conditional instruction if it is a branch
    if (insn_end_class == EndClass::Branch) {
        conditional_block.cond = true;
        conditional_block.branch_true = endif_block_id;
        conditional_block.branch_false = UNREACHABLE_BLOCK_ID;
    }
}

bool CFG::AnalyzeBranch(Block& block, FunctionId function_id, Location pc, Instruction inst,
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
        block.cond = IR::Condition(flow_test, static_cast<IR::Pred>(pred.index), pred.negated);
        block.branch_false = AddLabel(block, block.stack, pc + 1, function_id);
    } else {
        block.cond = true;
    }
    return true;
}

void CFG::AnalyzeBRA(Block& block, FunctionId function_id, Location pc, Instruction inst,
                     bool is_absolute) {
    const Location bra_pc{is_absolute ? inst.branch.Absolute() : BranchOffset(pc, inst)};
    block.branch_true = AddLabel(block, block.stack, bra_pc, function_id);
}

void CFG::AnalyzeBRX(Block&, Location, Instruction, bool is_absolute) {
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

CFG::AnalysisState CFG::AnalyzeEXIT(Block& block, FunctionId function_id, Location pc,
                                    Instruction inst) {
    const IR::FlowTest flow_test{inst.branch.flow_test};
    const Predicate pred{inst.Pred()};
    if (pred == Predicate{false} || flow_test == IR::FlowTest::F) {
        // EXIT will never be taken
        return AnalysisState::Continue;
    }
    if (pred != Predicate{true} || flow_test != IR::FlowTest::T) {
        if (block.stack.Peek(Token::PEXIT).has_value()) {
            throw NotImplementedException("Conditional EXIT with PEXIT token");
        }
        const IR::Condition cond{flow_test, static_cast<IR::Pred>(pred.index), pred.negated};
        AnalyzeCondInst(block, function_id, pc, EndClass::Exit, cond);
        return AnalysisState::Branch;
    }
    if (const std::optional<Location> exit_pc{block.stack.Peek(Token::PEXIT)}) {
        const Stack popped_stack{block.stack.Remove(Token::PEXIT)};
        block.cond = true;
        block.branch_true = AddLabel(block, popped_stack, *exit_pc, function_id);
        block.branch_false = UNREACHABLE_BLOCK_ID;
        return AnalysisState::Branch;
    }
    block.end = pc;
    block.end_class = EndClass::Exit;
    return AnalysisState::Branch;
}

BlockId CFG::AddLabel(const Block& block, Stack stack, Location pc, FunctionId function_id) {
    Function& function{functions[function_id]};
    if (block.begin == pc) {
        return block.id;
    }
    const auto target{std::ranges::find(function.blocks_data, pc, &Block::begin)};
    if (target != function.blocks_data.end()) {
        return target->id;
    }
    const BlockId block_id{++function.current_block_id};
    function.labels.push_back(Label{
        .address{pc},
        .block_id{block_id},
        .stack{std::move(stack)},
    });
    return block_id;
}

std::string CFG::Dot() const {
    int node_uid{0};

    std::string dot{"digraph shader {\n"};
    for (const Function& function : functions) {
        dot += fmt::format("\tsubgraph cluster_{} {{\n", function.entrypoint);
        dot += fmt::format("\t\tnode [style=filled];\n");
        for (const u32 block_index : function.blocks) {
            const Block& block{function.blocks_data[block_index]};
            const std::string name{NameOf(block)};
            const auto add_branch = [&](BlockId branch_id, bool add_label) {
                const auto it{std::ranges::find(function.blocks_data, branch_id, &Block::id)};
                dot += fmt::format("\t\t{}->", name);
                if (it == function.blocks_data.end()) {
                    dot += fmt::format("\"Unknown label {}\"", branch_id);
                } else {
                    dot += NameOf(*it);
                };
                if (add_label && block.cond != true && block.cond != false) {
                    dot += fmt::format(" [label=\"{}\"]", block.cond);
                }
                dot += '\n';
            };
            dot += fmt::format("\t\t{};\n", name);
            switch (block.end_class) {
            case EndClass::Branch:
                if (block.cond != false) {
                    add_branch(block.branch_true, true);
                }
                if (block.cond != true) {
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
            case EndClass::Unreachable:
                dot += fmt::format("\t\t{}->N{};\n", name, node_uid);
                dot += fmt::format(
                    "\t\tN{} [label=\"Unreachable\"][shape=square][style=stripped];\n", node_uid);
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
        if (functions.front().blocks.empty()) {
            dot += "Start;\n";
        } else {
            dot += fmt::format("\tStart -> {};\n", NameOf(functions.front().blocks_data.front()));
        }
        dot += fmt::format("\tStart [shape=diamond];\n");
    }
    dot += "}\n";
    return dot;
}

} // namespace Shader::Maxwell::Flow
