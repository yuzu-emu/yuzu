// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <span>

#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/frontend/maxwell/control_flow.h"
#include "shader_recompiler/frontend/maxwell/termination_code.h"

namespace Shader::Maxwell {

static void EmitExit(IR::IREmitter& ir) {
    ir.Exit();
}

static IR::U1 GetFlowTest(IR::FlowTest flow_test, IR::IREmitter& ir) {
    switch (flow_test) {
    case IR::FlowTest::T:
        return ir.Imm1(true);
    case IR::FlowTest::F:
        return ir.Imm1(false);
    case IR::FlowTest::NE:
        // FIXME: Verify this
        return ir.LogicalNot(ir.GetZFlag());
    case IR::FlowTest::NaN:
        // FIXME: Verify this
        return ir.LogicalAnd(ir.GetSFlag(), ir.GetZFlag());
    default:
        throw NotImplementedException("Flow test {}", flow_test);
    }
}

static IR::U1 GetCond(IR::Condition cond, IR::IREmitter& ir) {
    const IR::FlowTest flow_test{cond.FlowTest()};
    const auto [pred, pred_negated]{cond.Pred()};
    if (pred == IR::Pred::PT && !pred_negated) {
        return GetFlowTest(flow_test, ir);
    }
    if (flow_test == IR::FlowTest::T) {
        return ir.GetPred(pred, pred_negated);
    }
    return ir.LogicalAnd(ir.GetPred(pred, pred_negated), GetFlowTest(flow_test, ir));
}

static void EmitBranch(const Flow::Block& flow_block, std::span<IR::Block* const> block_map,
                       IR::IREmitter& ir) {
    const auto add_immediate_predecessor = [&](Flow::BlockId label) {
        block_map[label]->AddImmediatePredecessor(&ir.block);
    };
    if (flow_block.cond == true) {
        add_immediate_predecessor(flow_block.branch_true);
        return ir.Branch(block_map[flow_block.branch_true]);
    }
    if (flow_block.cond == false) {
        add_immediate_predecessor(flow_block.branch_false);
        return ir.Branch(block_map[flow_block.branch_false]);
    }
    add_immediate_predecessor(flow_block.branch_true);
    add_immediate_predecessor(flow_block.branch_false);
    return ir.BranchConditional(GetCond(flow_block.cond, ir), block_map[flow_block.branch_true],
                                block_map[flow_block.branch_false]);
}

void EmitTerminationCode(const Flow::Block& flow_block, std::span<IR::Block* const> block_map) {
    IR::Block* const block{block_map[flow_block.id]};
    IR::IREmitter ir(*block);
    switch (flow_block.end_class) {
    case Flow::EndClass::Branch:
        EmitBranch(flow_block, block_map, ir);
        break;
    case Flow::EndClass::Exit:
        EmitExit(ir);
        break;
    case Flow::EndClass::Return:
        ir.Return();
        break;
    case Flow::EndClass::Unreachable:
        ir.Unreachable();
        break;
    }
}

} // namespace Shader::Maxwell
