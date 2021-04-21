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

#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/frontend/maxwell/decode.h"
#include "shader_recompiler/frontend/maxwell/structured_control_flow.h"
#include "shader_recompiler/frontend/maxwell/translate/translate.h"
#include "shader_recompiler/object_pool.h"

namespace Shader::Maxwell {
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
    Kill,
    Unreachable,
    Function,
    Identity,
    Not,
    Or,
    SetVariable,
    SetIndirectBranchVariable,
    Variable,
    IndirectBranchCond,
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
struct Kill {};
struct Unreachable {};
struct FunctionTag {};
struct Identity {};
struct Not {};
struct Or {};
struct SetVariable {};
struct SetIndirectBranchVariable {};
struct Variable {};
struct IndirectBranchCond {};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 26495) // Always initialize a member variable, expected in Statement
#endif
struct Statement : ListBaseHook {
    Statement(IR::Block* code_, Statement* up_) : code{code_}, up{up_}, type{StatementType::Code} {}
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
    Statement(Kill) : type{StatementType::Kill} {}
    Statement(Unreachable) : type{StatementType::Unreachable} {}
    Statement(FunctionTag) : children{}, type{StatementType::Function} {}
    Statement(Identity, IR::Condition cond_) : guest_cond{cond_}, type{StatementType::Identity} {}
    Statement(Not, Statement* op_) : op{op_}, type{StatementType::Not} {}
    Statement(Or, Statement* op_a_, Statement* op_b_)
        : op_a{op_a_}, op_b{op_b_}, type{StatementType::Or} {}
    Statement(SetVariable, u32 id_, Statement* op_, Statement* up_)
        : op{op_}, id{id_}, up{up_}, type{StatementType::SetVariable} {}
    Statement(SetIndirectBranchVariable, IR::Reg branch_reg_, s32 branch_offset_)
        : branch_offset{branch_offset_},
          branch_reg{branch_reg_}, type{StatementType::SetIndirectBranchVariable} {}
    Statement(Variable, u32 id_) : id{id_}, type{StatementType::Variable} {}
    Statement(IndirectBranchCond, u32 location_)
        : location{location_}, type{StatementType::IndirectBranchCond} {}

    ~Statement() {
        if (HasChildren(type)) {
            std::destroy_at(&children);
        }
    }

    union {
        IR::Block* code;
        Node label;
        Tree children;
        IR::Condition guest_cond;
        Statement* op;
        Statement* op_a;
        u32 location;
        s32 branch_offset;
    };
    union {
        Statement* cond;
        Statement* op_b;
        u32 id;
        IR::Reg branch_reg;
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
    case StatementType::IndirectBranchCond:
        return fmt::format("(indirect_branch == {:x})", stmt->location);
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
            ret += fmt::format("{}    Block {:04x} -> {:04x} (0x{:016x});\n", indent,
                               stmt->code->LocationBegin(), stmt->code->LocationEnd(),
                               reinterpret_cast<uintptr_t>(stmt->code));
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
        case StatementType::Kill:
            ret += fmt::format("{}    kill;\n", indent);
            break;
        case StatementType::Unreachable:
            ret += fmt::format("{}    unreachable;\n", indent);
            break;
        case StatementType::SetVariable:
            ret += fmt::format("{}    goto_L{} = {};\n", indent, stmt->id, DumpExpr(stmt->op));
            break;
        case StatementType::SetIndirectBranchVariable:
            ret += fmt::format("{}    indirect_branch = {} + {};\n", indent, stmt->branch_reg,
                               stmt->branch_offset);
            break;
        case StatementType::Function:
        case StatementType::Identity:
        case StatementType::Not:
        case StatementType::Or:
        case StatementType::Variable:
        case StatementType::IndirectBranchCond:
            throw LogicError("Statement can't be printed");
        }
    }
    return ret;
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

[[maybe_unused]] bool AreSiblings(Node goto_stmt, Node label_stmt) noexcept {
    Node it{goto_stmt};
    do {
        if (it == label_stmt) {
            return true;
        }
        --it;
    } while (it != goto_stmt->up->children.begin());
    while (it != goto_stmt->up->children.end()) {
        if (it == label_stmt) {
            return true;
        }
        ++it;
    }
    return false;
}

Node SiblingFromNephew(Node uncle, Node nephew) noexcept {
    Statement* const parent{uncle->up};
    Statement* it{&*nephew};
    while (it->up != parent) {
        it = it->up;
    }
    return Tree::s_iterator_to(*it);
}

bool AreOrdered(Node left_sibling, Node right_sibling) noexcept {
    const Node end{right_sibling->up->children.end()};
    for (auto it = right_sibling; it != end; ++it) {
        if (it == left_sibling) {
            return false;
        }
    }
    return true;
}

bool NeedsLift(Node goto_stmt, Node label_stmt) noexcept {
    const Node sibling{SiblingFromNephew(goto_stmt, label_stmt)};
    return AreOrdered(sibling, goto_stmt);
}

class GotoPass {
public:
    explicit GotoPass(Flow::CFG& cfg, ObjectPool<IR::Inst>& inst_pool_,
                      ObjectPool<IR::Block>& block_pool_, ObjectPool<Statement>& stmt_pool)
        : inst_pool{inst_pool_}, block_pool{block_pool_}, pool{stmt_pool} {
        std::vector gotos{BuildTree(cfg)};
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
                if (NeedsLift(goto_stmt, label_stmt)) {
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
        // Expensive operation:
        // if (!AreSiblings(goto_stmt, label_stmt)) {
        //     throw LogicError("Goto is not a sibling with the label");
        // }
        // goto_stmt and label_stmt are guaranteed to be siblings, eliminate
        if (std::next(goto_stmt) == label_stmt) {
            // Simply eliminate the goto if the label is next to it
            goto_stmt->up->children.erase(goto_stmt);
        } else if (AreOrdered(goto_stmt, label_stmt)) {
            // Eliminate goto_stmt with a conditional
            EliminateAsConditional(goto_stmt, label_stmt);
        } else {
            // Eliminate goto_stmt with a loop
            EliminateAsLoop(goto_stmt, label_stmt);
        }
    }

    std::vector<Node> BuildTree(Flow::CFG& cfg) {
        u32 label_id{0};
        std::vector<Node> gotos;
        Flow::Function& first_function{cfg.Functions().front()};
        BuildTree(cfg, first_function, label_id, gotos, root_stmt.children.end(), std::nullopt);
        return gotos;
    }

    void BuildTree(Flow::CFG& cfg, Flow::Function& function, u32& label_id,
                   std::vector<Node>& gotos, Node function_insert_point,
                   std::optional<Node> return_label) {
        Statement* const false_stmt{pool.Create(Identity{}, IR::Condition{false})};
        Tree& root{root_stmt.children};
        std::unordered_map<Flow::Block*, Node> local_labels;
        local_labels.reserve(function.blocks.size());

        for (Flow::Block& block : function.blocks) {
            Statement* const label{pool.Create(Label{}, label_id, &root_stmt)};
            const Node label_it{root.insert(function_insert_point, *label)};
            local_labels.emplace(&block, label_it);
            ++label_id;
        }
        for (Flow::Block& block : function.blocks) {
            const Node label{local_labels.at(&block)};
            // Insertion point
            const Node ip{std::next(label)};

            // Reset goto variables before the first block and after its respective label
            const auto make_reset_variable{[&]() -> Statement& {
                return *pool.Create(SetVariable{}, label->id, false_stmt, &root_stmt);
            }};
            root.push_front(make_reset_variable());
            root.insert(ip, make_reset_variable());

            const u32 begin_offset{block.begin.Offset()};
            const u32 end_offset{block.end.Offset()};
            IR::Block* const ir_block{block_pool.Create(inst_pool, begin_offset, end_offset)};
            root.insert(ip, *pool.Create(ir_block, &root_stmt));

            switch (block.end_class) {
            case Flow::EndClass::Branch: {
                Statement* const always_cond{pool.Create(Identity{}, IR::Condition{true})};
                if (block.cond == IR::Condition{true}) {
                    const Node true_label{local_labels.at(block.branch_true)};
                    gotos.push_back(
                        root.insert(ip, *pool.Create(Goto{}, always_cond, true_label, &root_stmt)));
                } else if (block.cond == IR::Condition{false}) {
                    const Node false_label{local_labels.at(block.branch_false)};
                    gotos.push_back(root.insert(
                        ip, *pool.Create(Goto{}, always_cond, false_label, &root_stmt)));
                } else {
                    const Node true_label{local_labels.at(block.branch_true)};
                    const Node false_label{local_labels.at(block.branch_false)};
                    Statement* const true_cond{pool.Create(Identity{}, block.cond)};
                    gotos.push_back(
                        root.insert(ip, *pool.Create(Goto{}, true_cond, true_label, &root_stmt)));
                    gotos.push_back(root.insert(
                        ip, *pool.Create(Goto{}, always_cond, false_label, &root_stmt)));
                }
                break;
            }
            case Flow::EndClass::IndirectBranch:
                root.insert(ip, *pool.Create(SetIndirectBranchVariable{}, block.branch_reg,
                                             block.branch_offset));
                for (const Flow::IndirectBranch& indirect : block.indirect_branches) {
                    const Node indirect_label{local_labels.at(indirect.block)};
                    Statement* cond{pool.Create(IndirectBranchCond{}, indirect.address)};
                    Statement* goto_stmt{pool.Create(Goto{}, cond, indirect_label, &root_stmt)};
                    gotos.push_back(root.insert(ip, *goto_stmt));
                }
                root.insert(ip, *pool.Create(Unreachable{}));
                break;
            case Flow::EndClass::Call: {
                Flow::Function& call{cfg.Functions()[block.function_call]};
                const Node call_return_label{local_labels.at(block.return_block)};
                BuildTree(cfg, call, label_id, gotos, ip, call_return_label);
                break;
            }
            case Flow::EndClass::Exit:
                root.insert(ip, *pool.Create(Return{}));
                break;
            case Flow::EndClass::Return: {
                Statement* const always_cond{pool.Create(Identity{}, block.cond)};
                auto goto_stmt{pool.Create(Goto{}, always_cond, return_label.value(), &root_stmt)};
                gotos.push_back(root.insert(ip, *goto_stmt));
                break;
            }
            case Flow::EndClass::Kill:
                root.insert(ip, *pool.Create(Kill{}));
                break;
            }
        }
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
        const Node label{goto_stmt->label};
        const Node label_nested_stmt{SiblingFromNephew(goto_stmt, label)};
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

        switch (label_nested_stmt->type) {
        case StatementType::If:
            // Update nested if condition
            label_nested_stmt->cond = pool.Create(Or{}, variable, label_nested_stmt->cond);
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
        const Node label_nested_stmt{SiblingFromNephew(goto_stmt, label)};

        Tree loop_body;
        loop_body.splice(loop_body.begin(), body, label_nested_stmt, goto_stmt);
        SanitizeNoBreaks(loop_body);
        Statement* const variable{pool.Create(Variable{}, label_id)};
        Statement* const loop_stmt{pool.Create(Loop{}, variable, std::move(loop_body), parent)};
        UpdateTreeUp(loop_stmt);
        body.insert(goto_stmt, *loop_stmt);

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

    ObjectPool<IR::Inst>& inst_pool;
    ObjectPool<IR::Block>& block_pool;
    ObjectPool<Statement>& pool;
    Statement root_stmt{FunctionTag{}};
};

IR::Block* TryFindForwardBlock(const Statement& stmt) {
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

[[nodiscard]] IR::U1 VisitExpr(IR::IREmitter& ir, const Statement& stmt) {
    switch (stmt.type) {
    case StatementType::Identity:
        return ir.Condition(stmt.guest_cond);
    case StatementType::Not:
        return ir.LogicalNot(IR::U1{VisitExpr(ir, *stmt.op)});
    case StatementType::Or:
        return ir.LogicalOr(VisitExpr(ir, *stmt.op_a), VisitExpr(ir, *stmt.op_b));
    case StatementType::Variable:
        return ir.GetGotoVariable(stmt.id);
    case StatementType::IndirectBranchCond:
        return ir.IEqual(ir.GetIndirectBranchVariable(), ir.Imm32(stmt.location));
    default:
        throw NotImplementedException("Statement type {}", stmt.type);
    }
}

class TranslatePass {
public:
    TranslatePass(ObjectPool<IR::Inst>& inst_pool_, ObjectPool<IR::Block>& block_pool_,
                  ObjectPool<Statement>& stmt_pool_, Environment& env_, Statement& root_stmt,
                  IR::BlockList& block_list_)
        : stmt_pool{stmt_pool_}, inst_pool{inst_pool_}, block_pool{block_pool_}, env{env_},
          block_list{block_list_} {
        Visit(root_stmt, nullptr, nullptr);

        IR::Block& first_block{*block_list.front()};
        IR::IREmitter ir{first_block, first_block.begin()};
        ir.Prologue();
    }

private:
    void Visit(Statement& parent, IR::Block* continue_block, IR::Block* break_block) {
        Tree& tree{parent.children};
        IR::Block* current_block{nullptr};

        for (auto it = tree.begin(); it != tree.end(); ++it) {
            Statement& stmt{*it};
            switch (stmt.type) {
            case StatementType::Label:
                // Labels can be ignored
                break;
            case StatementType::Code: {
                if (current_block && current_block != stmt.code) {
                    IR::IREmitter{*current_block}.Branch(stmt.code);
                }
                current_block = stmt.code;
                Translate(env, stmt.code);
                block_list.push_back(stmt.code);
                break;
            }
            case StatementType::SetVariable: {
                if (!current_block) {
                    current_block = MergeBlock(parent, stmt);
                }
                IR::IREmitter ir{*current_block};
                ir.SetGotoVariable(stmt.id, VisitExpr(ir, *stmt.op));
                break;
            }
            case StatementType::SetIndirectBranchVariable: {
                if (!current_block) {
                    current_block = MergeBlock(parent, stmt);
                }
                IR::IREmitter ir{*current_block};
                IR::U32 address{ir.IAdd(ir.GetReg(stmt.branch_reg), ir.Imm32(stmt.branch_offset))};
                ir.SetIndirectBranchVariable(address);
                break;
            }
            case StatementType::If: {
                if (!current_block) {
                    current_block = block_pool.Create(inst_pool);
                    block_list.push_back(current_block);
                }
                IR::Block* const merge_block{MergeBlock(parent, stmt)};

                // Visit children
                const size_t first_block_index{block_list.size()};
                Visit(stmt, merge_block, break_block);

                // Implement if header block
                IR::Block* const first_if_block{block_list.at(first_block_index)};
                IR::IREmitter ir{*current_block};
                const IR::U1 cond{VisitExpr(ir, *stmt.cond)};
                ir.SelectionMerge(merge_block);
                ir.BranchConditional(cond, first_if_block, merge_block);

                current_block = merge_block;
                break;
            }
            case StatementType::Loop: {
                IR::Block* const loop_header_block{block_pool.Create(inst_pool)};
                if (current_block) {
                    IR::IREmitter{*current_block}.Branch(loop_header_block);
                }
                block_list.push_back(loop_header_block);

                IR::Block* const new_continue_block{block_pool.Create(inst_pool)};
                IR::Block* const merge_block{MergeBlock(parent, stmt)};

                // Visit children
                const size_t first_block_index{block_list.size()};
                Visit(stmt, new_continue_block, merge_block);

                // The continue block is located at the end of the loop
                block_list.push_back(new_continue_block);

                // Implement loop header block
                IR::Block* const first_loop_block{block_list.at(first_block_index)};
                IR::IREmitter ir{*loop_header_block};
                ir.LoopMerge(merge_block, new_continue_block);
                ir.Branch(first_loop_block);

                // Implement continue block
                IR::IREmitter continue_ir{*new_continue_block};
                const IR::U1 continue_cond{VisitExpr(continue_ir, *stmt.cond)};
                continue_ir.BranchConditional(continue_cond, ir.block, merge_block);

                current_block = merge_block;
                break;
            }
            case StatementType::Break: {
                if (!current_block) {
                    current_block = block_pool.Create(inst_pool);
                    block_list.push_back(current_block);
                }
                IR::Block* const skip_block{MergeBlock(parent, stmt)};

                IR::IREmitter ir{*current_block};
                ir.BranchConditional(VisitExpr(ir, *stmt.cond), break_block, skip_block);

                current_block = skip_block;
                break;
            }
            case StatementType::Return: {
                if (!current_block) {
                    current_block = block_pool.Create(inst_pool);
                    block_list.push_back(current_block);
                }
                IR::IREmitter ir{*current_block};
                ir.Epilogue();
                ir.Return();
                current_block = nullptr;
                break;
            }
            case StatementType::Kill: {
                if (!current_block) {
                    current_block = block_pool.Create(inst_pool);
                    block_list.push_back(current_block);
                }
                IR::Block* demote_block{MergeBlock(parent, stmt)};
                IR::IREmitter{*current_block}.DemoteToHelperInvocation(demote_block);
                current_block = demote_block;
                break;
            }
            case StatementType::Unreachable: {
                if (!current_block) {
                    current_block = block_pool.Create(inst_pool);
                    block_list.push_back(current_block);
                }
                IR::IREmitter{*current_block}.Unreachable();
                current_block = nullptr;
                break;
            }
            default:
                throw NotImplementedException("Statement type {}", stmt.type);
            }
        }
        if (current_block) {
            IR::IREmitter ir{*current_block};
            if (continue_block) {
                ir.Branch(continue_block);
            } else {
                ir.Unreachable();
            }
        }
    }

    IR::Block* MergeBlock(Statement& parent, Statement& stmt) {
        if (IR::Block* const block{TryFindForwardBlock(stmt)}) {
            return block;
        }
        // Create a merge block we can visit later
        IR::Block* const block{block_pool.Create(inst_pool)};
        Statement* const merge_stmt{stmt_pool.Create(block, &parent)};
        parent.children.insert(std::next(Tree::s_iterator_to(stmt)), *merge_stmt);
        return block;
    }

    ObjectPool<Statement>& stmt_pool;
    ObjectPool<IR::Inst>& inst_pool;
    ObjectPool<IR::Block>& block_pool;
    Environment& env;
    IR::BlockList& block_list;
};
} // Anonymous namespace

IR::BlockList VisitAST(ObjectPool<IR::Inst>& inst_pool, ObjectPool<IR::Block>& block_pool,
                       Environment& env, Flow::CFG& cfg) {
    ObjectPool<Statement> stmt_pool{64};
    GotoPass goto_pass{cfg, inst_pool, block_pool, stmt_pool};
    Statement& root{goto_pass.RootStatement()};
    IR::BlockList block_list;
    TranslatePass{inst_pool, block_pool, stmt_pool, env, root, block_list};
    return block_list;
}

} // namespace Shader::Maxwell
