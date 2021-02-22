// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include <boost/intrusive/list.hpp>

#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/object_pool.h"

namespace Shader::IR {
namespace {
struct Statement;

// Use normal_link because we are not guaranteed to destroy the tree in order
using ListBaseHook =
    boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>;

using Tree = boost::intrusive::list<Statement,
                                    // Allow using Statement without a definition
                                    boost::intrusive::base_hook<ListBaseHook>,
                                    // Avoid linear complexity on splice, size is never called
                                    boost::intrusive::constant_time_size<false>>;
using Node = Tree::iterator;
using ConstNode = Tree::const_iterator;

enum class StatementType {
    Code,
    Goto,
    Label,
    If,
    Loop,
    Break,
    Return,
    Function,
    Identity,
    Not,
    Or,
    SetVariable,
    Variable,
};

bool HasChildren(StatementType type) {
    switch (type) {
    case StatementType::If:
    case StatementType::Loop:
    case StatementType::Function:
        return true;
    default:
        return false;
    }
}

struct Goto {};
struct Label {};
struct If {};
struct Loop {};
struct Break {};
struct Return {};
struct FunctionTag {};
struct Identity {};
struct Not {};
struct Or {};
struct SetVariable {};
struct Variable {};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 26495) // Always initialize a member variable, expected in Statement
#endif
struct Statement : ListBaseHook {
    Statement(Block* code_, Statement* up_) : code{code_}, up{up_}, type{StatementType::Code} {}
    Statement(Goto, Statement* cond_, Node label_, Statement* up_)
        : label{label_}, cond{cond_}, up{up_}, type{StatementType::Goto} {}
    Statement(Label, u32 id_, Statement* up_) : id{id_}, up{up_}, type{StatementType::Label} {}
    Statement(If, Statement* cond_, Tree&& children_, Statement* up_)
        : children{std::move(children_)}, cond{cond_}, up{up_}, type{StatementType::If} {}
    Statement(Loop, Statement* cond_, Tree&& children_, Statement* up_)
        : children{std::move(children_)}, cond{cond_}, up{up_}, type{StatementType::Loop} {}
    Statement(Break, Statement* cond_, Statement* up_)
        : cond{cond_}, up{up_}, type{StatementType::Break} {}
    Statement(Return) : type{StatementType::Return} {}
    Statement(FunctionTag) : children{}, type{StatementType::Function} {}
    Statement(Identity, Condition cond_) : guest_cond{cond_}, type{StatementType::Identity} {}
    Statement(Not, Statement* op_) : op{op_}, type{StatementType::Not} {}
    Statement(Or, Statement* op_a_, Statement* op_b_)
        : op_a{op_a_}, op_b{op_b_}, type{StatementType::Or} {}
    Statement(SetVariable, u32 id_, Statement* op_, Statement* up_)
        : op{op_}, id{id_}, up{up_}, type{StatementType::SetVariable} {}
    Statement(Variable, u32 id_) : id{id_}, type{StatementType::Variable} {}

    ~Statement() {
        if (HasChildren(type)) {
            std::destroy_at(&children);
        }
    }

    union {
        Block* code;
        Node label;
        Tree children;
        Condition guest_cond;
        Statement* op;
        Statement* op_a;
    };
    union {
        Statement* cond;
        Statement* op_b;
        u32 id;
    };
    Statement* up{};
    StatementType type;
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

std::string DumpExpr(const Statement* stmt) {
    switch (stmt->type) {
    case StatementType::Identity:
        return fmt::format("{}", stmt->guest_cond);
    case StatementType::Not:
        return fmt::format("!{}", DumpExpr(stmt->op));
    case StatementType::Or:
        return fmt::format("{} || {}", DumpExpr(stmt->op_a), DumpExpr(stmt->op_b));
    case StatementType::Variable:
        return fmt::format("goto_L{}", stmt->id);
    default:
        return "<invalid type>";
    }
}

std::string DumpTree(const Tree& tree, u32 indentation = 0) {
    std::string ret;
    std::string indent(indentation, ' ');
    for (auto stmt = tree.begin(); stmt != tree.end(); ++stmt) {
        switch (stmt->type) {
        case StatementType::Code:
            ret += fmt::format("{}    Block {:04x};\n", indent, stmt->code->LocationBegin());
            break;
        case StatementType::Goto:
            ret += fmt::format("{}    if ({}) goto L{};\n", indent, DumpExpr(stmt->cond),
                               stmt->label->id);
            break;
        case StatementType::Label:
            ret += fmt::format("{}L{}:\n", indent, stmt->id);
            break;
        case StatementType::If:
            ret += fmt::format("{}    if ({}) {{\n", indent, DumpExpr(stmt->cond));
            ret += DumpTree(stmt->children, indentation + 4);
            ret += fmt::format("{}    }}\n", indent);
            break;
        case StatementType::Loop:
            ret += fmt::format("{}    do {{\n", indent);
            ret += DumpTree(stmt->children, indentation + 4);
            ret += fmt::format("{}    }} while ({});\n", indent, DumpExpr(stmt->cond));
            break;
        case StatementType::Break:
            ret += fmt::format("{}    if ({}) break;\n", indent, DumpExpr(stmt->cond));
            break;
        case StatementType::Return:
            ret += fmt::format("{}    return;\n", indent);
            break;
        case StatementType::SetVariable:
            ret += fmt::format("{}    goto_L{} = {};\n", indent, stmt->id, DumpExpr(stmt->op));
            break;
        case StatementType::Function:
        case StatementType::Identity:
        case StatementType::Not:
        case StatementType::Or:
        case StatementType::Variable:
            throw LogicError("Statement can't be printed");
        }
    }
    return ret;
}

bool HasNode(const Tree& tree, ConstNode stmt) {
    const auto end{tree.end()};
    for (auto it = tree.begin(); it != end; ++it) {
        if (it == stmt || (HasChildren(it->type) && HasNode(it->children, stmt))) {
            return true;
        }
    }
    return false;
}

Node FindStatementWithLabel(Tree& tree, ConstNode goto_stmt) {
    const ConstNode label_stmt{goto_stmt->label};
    const ConstNode end{tree.end()};
    for (auto it = tree.begin(); it != end; ++it) {
        if (it == label_stmt || (HasChildren(it->type) && HasNode(it->children, label_stmt))) {
            return it;
        }
    }
    throw LogicError("Lift label not in tree");
}

void SanitizeNoBreaks(const Tree& tree) {
    if (std::ranges::find(tree, StatementType::Break, &Statement::type) != tree.end()) {
        throw NotImplementedException("Capturing statement with break nodes");
    }
}

size_t Level(Node stmt) {
    size_t level{0};
    Statement* node{stmt->up};
    while (node) {
        ++level;
        node = node->up;
    }
    return level;
}

bool IsDirectlyRelated(Node goto_stmt, Node label_stmt) {
    const size_t goto_level{Level(goto_stmt)};
    const size_t label_level{Level(label_stmt)};
    size_t min_level;
    size_t max_level;
    Node min;
    Node max;
    if (label_level < goto_level) {
        min_level = label_level;
        max_level = goto_level;
        min = label_stmt;
        max = goto_stmt;
    } else { // goto_level < label_level
        min_level = goto_level;
        max_level = label_level;
        min = goto_stmt;
        max = label_stmt;
    }
    while (max_level > min_level) {
        --max_level;
        max = max->up;
    }
    return min->up == max->up;
}

bool IsIndirectlyRelated(Node goto_stmt, Node label_stmt) {
    return goto_stmt->up != label_stmt->up && !IsDirectlyRelated(goto_stmt, label_stmt);
}

bool SearchNode(const Tree& tree, ConstNode stmt, size_t& offset) {
    ++offset;

    const auto end = tree.end();
    for (ConstNode it = tree.begin(); it != end; ++it) {
        ++offset;
        if (stmt == it) {
            return true;
        }
        if (HasChildren(it->type) && SearchNode(it->children, stmt, offset)) {
            return true;
        }
    }
    return false;
}

class GotoPass {
public:
    explicit GotoPass(std::span<Block* const> blocks, ObjectPool<Statement>& stmt_pool)
        : pool{stmt_pool} {
        std::vector gotos{BuildUnorderedTreeGetGotos(blocks)};
        for (const Node& goto_stmt : gotos | std::views::reverse) {
            RemoveGoto(goto_stmt);
        }
    }

    Statement& RootStatement() noexcept {
        return root_stmt;
    }

private:
    void RemoveGoto(Node goto_stmt) {
        // Force goto_stmt and label_stmt to be directly related
        const Node label_stmt{goto_stmt->label};
        if (IsIndirectlyRelated(goto_stmt, label_stmt)) {
            // Move goto_stmt out using outward-movement transformation until it becomes
            // directly related to label_stmt
            while (!IsDirectlyRelated(goto_stmt, label_stmt)) {
                goto_stmt = MoveOutward(goto_stmt);
            }
        }
        // Force goto_stmt and label_stmt to be siblings
        if (IsDirectlyRelated(goto_stmt, label_stmt)) {
            const size_t label_level{Level(label_stmt)};
            size_t goto_level{Level(goto_stmt)};
            if (goto_level > label_level) {
                // Move goto_stmt out of its level using outward-movement transformations
                while (goto_level > label_level) {
                    goto_stmt = MoveOutward(goto_stmt);
                    --goto_level;
                }
            } else { // Level(goto_stmt) < Level(label_stmt)
                if (Offset(goto_stmt) > Offset(label_stmt)) {
                    // Lift goto_stmt to above stmt containing label_stmt using goto-lifting
                    // transformations
                    goto_stmt = Lift(goto_stmt);
                }
                // Move goto_stmt into label_stmt's level using inward-movement transformation
                while (goto_level < label_level) {
                    goto_stmt = MoveInward(goto_stmt);
                    ++goto_level;
                }
            }
        }
        // TODO: Remove this
        Node it{goto_stmt};
        bool sibling{false};
        do {
            sibling |= it == label_stmt;
            --it;
        } while (it != goto_stmt->up->children.begin());
        while (it != goto_stmt->up->children.end()) {
            sibling |= it == label_stmt;
            ++it;
        }
        if (!sibling) {
            throw LogicError("Not siblings");
        }

        // goto_stmt and label_stmt are guaranteed to be siblings, eliminate
        if (std::next(goto_stmt) == label_stmt) {
            // Simply eliminate the goto if the label is next to it
            goto_stmt->up->children.erase(goto_stmt);
        } else if (Offset(goto_stmt) < Offset(label_stmt)) {
            // Eliminate goto_stmt with a conditional
            EliminateAsConditional(goto_stmt, label_stmt);
        } else {
            // Eliminate goto_stmt with a loop
            EliminateAsLoop(goto_stmt, label_stmt);
        }
    }

    std::vector<Node> BuildUnorderedTreeGetGotos(std::span<Block* const> blocks) {
        // Assume all blocks have two branches
        std::vector<Node> gotos;
        gotos.reserve(blocks.size() * 2);

        const std::unordered_map labels_map{BuildLabels(blocks)};
        Tree& root{root_stmt.children};
        auto insert_point{root.begin()};
        for (Block* const block : blocks) {
            ++insert_point; // Skip label
            ++insert_point; // Skip set variable
            root.insert(insert_point, *pool.Create(block, &root_stmt));

            if (block->IsTerminationBlock()) {
                root.insert(insert_point, *pool.Create(Return{}));
                continue;
            }
            const Condition cond{block->BranchCondition()};
            Statement* const true_cond{pool.Create(Identity{}, Condition{true})};
            if (cond == Condition{true} || cond == Condition{false}) {
                const bool is_true{cond == Condition{true}};
                const Block* const branch{is_true ? block->TrueBranch() : block->FalseBranch()};
                const Node label{labels_map.at(branch)};
                Statement* const goto_stmt{pool.Create(Goto{}, true_cond, label, &root_stmt)};
                gotos.push_back(root.insert(insert_point, *goto_stmt));
            } else {
                Statement* const ident_cond{pool.Create(Identity{}, cond)};
                const Node true_label{labels_map.at(block->TrueBranch())};
                const Node false_label{labels_map.at(block->FalseBranch())};
                Statement* goto_true{pool.Create(Goto{}, ident_cond, true_label, &root_stmt)};
                Statement* goto_false{pool.Create(Goto{}, true_cond, false_label, &root_stmt)};
                gotos.push_back(root.insert(insert_point, *goto_true));
                gotos.push_back(root.insert(insert_point, *goto_false));
            }
        }
        return gotos;
    }

    std::unordered_map<const Block*, Node> BuildLabels(std::span<Block* const> blocks) {
        // TODO: Consider storing labels intrusively inside the block
        std::unordered_map<const Block*, Node> labels_map;
        Tree& root{root_stmt.children};
        u32 label_id{0};
        for (const Block* const block : blocks) {
            Statement* const label{pool.Create(Label{}, label_id, &root_stmt)};
            labels_map.emplace(block, root.insert(root.end(), *label));
            Statement* const false_stmt{pool.Create(Identity{}, Condition{false})};
            root.push_back(*pool.Create(SetVariable{}, label_id, false_stmt, &root_stmt));
            ++label_id;
        }
        return labels_map;
    }

    void UpdateTreeUp(Statement* tree) {
        for (Statement& stmt : tree->children) {
            stmt.up = tree;
        }
    }

    void EliminateAsConditional(Node goto_stmt, Node label_stmt) {
        Tree& body{goto_stmt->up->children};
        Tree if_body;
        if_body.splice(if_body.begin(), body, std::next(goto_stmt), label_stmt);
        Statement* const cond{pool.Create(Not{}, goto_stmt->cond)};
        Statement* const if_stmt{pool.Create(If{}, cond, std::move(if_body), goto_stmt->up)};
        UpdateTreeUp(if_stmt);
        body.insert(goto_stmt, *if_stmt);
        body.erase(goto_stmt);
    }

    void EliminateAsLoop(Node goto_stmt, Node label_stmt) {
        Tree& body{goto_stmt->up->children};
        Tree loop_body;
        loop_body.splice(loop_body.begin(), body, label_stmt, goto_stmt);
        Statement* const cond{goto_stmt->cond};
        Statement* const loop{pool.Create(Loop{}, cond, std::move(loop_body), goto_stmt->up)};
        UpdateTreeUp(loop);
        body.insert(goto_stmt, *loop);
        body.erase(goto_stmt);
    }

    [[nodiscard]] Node MoveOutward(Node goto_stmt) {
        switch (goto_stmt->up->type) {
        case StatementType::If:
            return MoveOutwardIf(goto_stmt);
        case StatementType::Loop:
            return MoveOutwardLoop(goto_stmt);
        default:
            throw LogicError("Invalid outward movement");
        }
    }

    [[nodiscard]] Node MoveInward(Node goto_stmt) {
        Statement* const parent{goto_stmt->up};
        Tree& body{parent->children};
        const Node label_nested_stmt{FindStatementWithLabel(body, goto_stmt)};
        const Node label{goto_stmt->label};
        const u32 label_id{label->id};

        Statement* const goto_cond{goto_stmt->cond};
        Statement* const set_var{pool.Create(SetVariable{}, label_id, goto_cond, parent)};
        body.insert(goto_stmt, *set_var);

        Tree if_body;
        if_body.splice(if_body.begin(), body, std::next(goto_stmt), label_nested_stmt);
        Statement* const variable{pool.Create(Variable{}, label_id)};
        Statement* const neg_var{pool.Create(Not{}, variable)};
        if (!if_body.empty()) {
            Statement* const if_stmt{pool.Create(If{}, neg_var, std::move(if_body), parent)};
            UpdateTreeUp(if_stmt);
            body.insert(goto_stmt, *if_stmt);
        }
        body.erase(goto_stmt);

        // Update nested if condition
        switch (label_nested_stmt->type) {
        case StatementType::If:
            label_nested_stmt->cond = pool.Create(Or{}, neg_var, label_nested_stmt->cond);
            break;
        case StatementType::Loop:
            break;
        default:
            throw LogicError("Invalid inward movement");
        }
        Tree& nested_tree{label_nested_stmt->children};
        Statement* const new_goto{pool.Create(Goto{}, variable, label, &*label_nested_stmt)};
        return nested_tree.insert(nested_tree.begin(), *new_goto);
    }

    [[nodiscard]] Node Lift(Node goto_stmt) {
        Statement* const parent{goto_stmt->up};
        Tree& body{parent->children};
        const Node label{goto_stmt->label};
        const u32 label_id{label->id};
        const Node label_nested_stmt{FindStatementWithLabel(body, goto_stmt)};
        const auto type{label_nested_stmt->type};

        Tree loop_body;
        loop_body.splice(loop_body.begin(), body, label_nested_stmt, goto_stmt);
        SanitizeNoBreaks(loop_body);
        Statement* const variable{pool.Create(Variable{}, label_id)};
        Statement* const loop_stmt{pool.Create(Loop{}, variable, std::move(loop_body), parent)};
        UpdateTreeUp(loop_stmt);
        const Node loop_node{body.insert(goto_stmt, *loop_stmt)};

        Statement* const new_goto{pool.Create(Goto{}, variable, label, loop_stmt)};
        loop_stmt->children.push_front(*new_goto);
        const Node new_goto_node{loop_stmt->children.begin()};

        Statement* const set_var{pool.Create(SetVariable{}, label_id, goto_stmt->cond, loop_stmt)};
        loop_stmt->children.push_back(*set_var);

        body.erase(goto_stmt);
        return new_goto_node;
    }

    Node MoveOutwardIf(Node goto_stmt) {
        const Node parent{Tree::s_iterator_to(*goto_stmt->up)};
        Tree& body{parent->children};
        const u32 label_id{goto_stmt->label->id};
        Statement* const goto_cond{goto_stmt->cond};
        Statement* const set_goto_var{pool.Create(SetVariable{}, label_id, goto_cond, &*parent)};
        body.insert(goto_stmt, *set_goto_var);

        Tree if_body;
        if_body.splice(if_body.begin(), body, std::next(goto_stmt), body.end());
        if_body.pop_front();
        Statement* const cond{pool.Create(Variable{}, label_id)};
        Statement* const neg_cond{pool.Create(Not{}, cond)};
        Statement* const if_stmt{pool.Create(If{}, neg_cond, std::move(if_body), &*parent)};
        UpdateTreeUp(if_stmt);
        body.insert(goto_stmt, *if_stmt);

        body.erase(goto_stmt);

        Statement* const new_cond{pool.Create(Variable{}, label_id)};
        Statement* const new_goto{pool.Create(Goto{}, new_cond, goto_stmt->label, parent->up)};
        Tree& parent_tree{parent->up->children};
        return parent_tree.insert(std::next(parent), *new_goto);
    }

    Node MoveOutwardLoop(Node goto_stmt) {
        Statement* const parent{goto_stmt->up};
        Tree& body{parent->children};
        const u32 label_id{goto_stmt->label->id};
        Statement* const goto_cond{goto_stmt->cond};
        Statement* const set_goto_var{pool.Create(SetVariable{}, label_id, goto_cond, parent)};
        Statement* const cond{pool.Create(Variable{}, label_id)};
        Statement* const break_stmt{pool.Create(Break{}, cond, parent)};
        body.insert(goto_stmt, *set_goto_var);
        body.insert(goto_stmt, *break_stmt);
        body.erase(goto_stmt);

        const Node loop{Tree::s_iterator_to(*goto_stmt->up)};
        Statement* const new_goto_cond{pool.Create(Variable{}, label_id)};
        Statement* const new_goto{pool.Create(Goto{}, new_goto_cond, goto_stmt->label, loop->up)};
        Tree& parent_tree{loop->up->children};
        return parent_tree.insert(std::next(loop), *new_goto);
    }

    size_t Offset(ConstNode stmt) const {
        size_t offset{0};
        if (!SearchNode(root_stmt.children, stmt, offset)) {
            throw LogicError("Node not found in tree");
        }
        return offset;
    }

    ObjectPool<Statement>& pool;
    Statement root_stmt{FunctionTag{}};
};

Block* TryFindForwardBlock(const Statement& stmt) {
    const Tree& tree{stmt.up->children};
    const ConstNode end{tree.cend()};
    ConstNode forward_node{std::next(Tree::s_iterator_to(stmt))};
    while (forward_node != end && !HasChildren(forward_node->type)) {
        if (forward_node->type == StatementType::Code) {
            return forward_node->code;
        }
        ++forward_node;
    }
    return nullptr;
}

[[nodiscard]] U1 VisitExpr(IREmitter& ir, const Statement& stmt) {
    switch (stmt.type) {
    case StatementType::Identity:
        return ir.Condition(stmt.guest_cond);
    case StatementType::Not:
        return ir.LogicalNot(U1{VisitExpr(ir, *stmt.op)});
    case StatementType::Or:
        return ir.LogicalOr(VisitExpr(ir, *stmt.op_a), VisitExpr(ir, *stmt.op_b));
    case StatementType::Variable:
        return ir.GetGotoVariable(stmt.id);
    default:
        throw NotImplementedException("Statement type {}", stmt.type);
    }
}

class TranslatePass {
public:
    TranslatePass(ObjectPool<Inst>& inst_pool_, ObjectPool<Block>& block_pool_,
                  ObjectPool<Statement>& stmt_pool_, Statement& root_stmt,
                  const std::function<void(IR::Block*)>& func_, BlockList& block_list_)
        : stmt_pool{stmt_pool_}, inst_pool{inst_pool_}, block_pool{block_pool_}, func{func_},
          block_list{block_list_} {
        Visit(root_stmt, nullptr, nullptr);
    }

private:
    void Visit(Statement& parent, Block* continue_block, Block* break_block) {
        Tree& tree{parent.children};
        Block* current_block{nullptr};

        for (auto it = tree.begin(); it != tree.end(); ++it) {
            Statement& stmt{*it};
            switch (stmt.type) {
            case StatementType::Label:
                // Labels can be ignored
                break;
            case StatementType::Code: {
                if (current_block && current_block != stmt.code) {
                    IREmitter ir{*current_block};
                    ir.Branch(stmt.code);
                }
                current_block = stmt.code;
                func(stmt.code);
                block_list.push_back(stmt.code);
                break;
            }
            case StatementType::SetVariable: {
                if (!current_block) {
                    current_block = MergeBlock(parent, stmt);
                }
                IREmitter ir{*current_block};
                ir.SetGotoVariable(stmt.id, VisitExpr(ir, *stmt.op));
                break;
            }
            case StatementType::If: {
                if (!current_block) {
                    current_block = block_pool.Create(inst_pool);
                    block_list.push_back(current_block);
                }
                Block* const merge_block{MergeBlock(parent, stmt)};

                // Visit children
                const size_t first_block_index{block_list.size()};
                Visit(stmt, merge_block, break_block);

                // Implement if header block
                Block* const first_if_block{block_list.at(first_block_index)};
                IREmitter ir{*current_block};
                const U1 cond{VisitExpr(ir, *stmt.cond)};
                ir.SelectionMerge(merge_block);
                ir.BranchConditional(cond, first_if_block, merge_block);

                current_block = merge_block;
                break;
            }
            case StatementType::Loop: {
                Block* const loop_header_block{block_pool.Create(inst_pool)};
                if (current_block) {
                    IREmitter{*current_block}.Branch(loop_header_block);
                }
                block_list.push_back(loop_header_block);

                Block* const new_continue_block{block_pool.Create(inst_pool)};
                Block* const merge_block{MergeBlock(parent, stmt)};

                // Visit children
                const size_t first_block_index{block_list.size()};
                Visit(stmt, new_continue_block, merge_block);

                // The continue block is located at the end of the loop
                block_list.push_back(new_continue_block);

                // Implement loop header block
                Block* const first_loop_block{block_list.at(first_block_index)};
                IREmitter ir{*loop_header_block};
                ir.LoopMerge(merge_block, new_continue_block);
                ir.Branch(first_loop_block);

                // Implement continue block
                IREmitter continue_ir{*new_continue_block};
                const U1 continue_cond{VisitExpr(continue_ir, *stmt.cond)};
                continue_ir.BranchConditional(continue_cond, ir.block, merge_block);

                current_block = merge_block;
                break;
            }
            case StatementType::Break: {
                if (!current_block) {
                    current_block = block_pool.Create(inst_pool);
                    block_list.push_back(current_block);
                }
                Block* const skip_block{MergeBlock(parent, stmt)};

                IREmitter ir{*current_block};
                ir.BranchConditional(VisitExpr(ir, *stmt.cond), break_block, skip_block);

                current_block = skip_block;
                break;
            }
            case StatementType::Return: {
                if (!current_block) {
                    current_block = block_pool.Create(inst_pool);
                    block_list.push_back(current_block);
                }
                IREmitter{*current_block}.Return();
                current_block = nullptr;
                break;
            }
            default:
                throw NotImplementedException("Statement type {}", stmt.type);
            }
        }
        if (current_block && continue_block) {
            IREmitter ir{*current_block};
            ir.Branch(continue_block);
        }
    }

    Block* MergeBlock(Statement& parent, Statement& stmt) {
        if (Block* const block{TryFindForwardBlock(stmt)}) {
            return block;
        }
        // Create a merge block we can visit later
        Block* const block{block_pool.Create(inst_pool)};
        Statement* const merge_stmt{stmt_pool.Create(block, &parent)};
        parent.children.insert(std::next(Tree::s_iterator_to(stmt)), *merge_stmt);
        return block;
    }

    ObjectPool<Statement>& stmt_pool;
    ObjectPool<Inst>& inst_pool;
    ObjectPool<Block>& block_pool;
    const std::function<void(IR::Block*)>& func;
    BlockList& block_list;
};
} // Anonymous namespace

BlockList VisitAST(ObjectPool<Inst>& inst_pool, ObjectPool<Block>& block_pool,
                   std::span<Block* const> unordered_blocks,
                   const std::function<void(Block*)>& func) {
    ObjectPool<Statement> stmt_pool{64};
    GotoPass goto_pass{unordered_blocks, stmt_pool};
    BlockList block_list;
    TranslatePass translate_pass{inst_pool, block_pool, stmt_pool, goto_pass.RootStatement(),
                                 func,      block_list};
    return block_list;
}

} // namespace Shader::IR
