// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// This file implements the SSA rewriting algorithm proposed in
//
//      Simple and Efficient Construction of Static Single Assignment Form.
//      Braun M., Buchwald S., Hack S., Leiﬂa R., Mallon C., Zwinkau A. (2013)
//      In: Jhala R., De Bosschere K. (eds)
//      Compiler Construction. CC 2013.
//      Lecture Notes in Computer Science, vol 7791.
//      Springer, Berlin, Heidelberg
//
//      https://link.springer.com/chapter/10.1007/978-3-642-37051-9_6
//

#include <boost/container/flat_map.hpp>

#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/function.h"
#include "shader_recompiler/frontend/ir/microinstruction.h"
#include "shader_recompiler/frontend/ir/opcode.h"
#include "shader_recompiler/frontend/ir/pred.h"
#include "shader_recompiler/frontend/ir/reg.h"
#include "shader_recompiler/ir_opt/passes.h"

namespace Shader::Optimization {
namespace {
using ValueMap = boost::container::flat_map<IR::Block*, IR::Value, std::less<IR::Block*>>;

struct FlagTag {};
struct ZeroFlagTag : FlagTag {};
struct SignFlagTag : FlagTag {};
struct CarryFlagTag : FlagTag {};
struct OverflowFlagTag : FlagTag {};

struct DefTable {
    [[nodiscard]] ValueMap& operator[](IR::Reg variable) noexcept {
        return regs[IR::RegIndex(variable)];
    }

    [[nodiscard]] ValueMap& operator[](IR::Pred variable) noexcept {
        return preds[IR::PredIndex(variable)];
    }

    [[nodiscard]] ValueMap& operator[](ZeroFlagTag) noexcept {
        return zero_flag;
    }

    [[nodiscard]] ValueMap& operator[](SignFlagTag) noexcept {
        return sign_flag;
    }

    [[nodiscard]] ValueMap& operator[](CarryFlagTag) noexcept {
        return carry_flag;
    }

    [[nodiscard]] ValueMap& operator[](OverflowFlagTag) noexcept {
        return overflow_flag;
    }

    std::array<ValueMap, IR::NUM_USER_REGS> regs;
    std::array<ValueMap, IR::NUM_USER_PREDS> preds;
    ValueMap zero_flag;
    ValueMap sign_flag;
    ValueMap carry_flag;
    ValueMap overflow_flag;
};

IR::Opcode UndefOpcode(IR::Reg) noexcept {
    return IR::Opcode::Undef32;
}

IR::Opcode UndefOpcode(IR::Pred) noexcept {
    return IR::Opcode::Undef1;
}

IR::Opcode UndefOpcode(const FlagTag&) noexcept {
    return IR::Opcode::Undef1;
}

[[nodiscard]] bool IsPhi(const IR::Inst& inst) noexcept {
    return inst.Opcode() == IR::Opcode::Phi;
}

class Pass {
public:
    void WriteVariable(auto variable, IR::Block* block, const IR::Value& value) {
        current_def[variable].insert_or_assign(block, value);
    }

    IR::Value ReadVariable(auto variable, IR::Block* block) {
        auto& def{current_def[variable]};
        if (const auto it{def.find(block)}; it != def.end()) {
            return it->second;
        }
        return ReadVariableRecursive(variable, block);
    }

private:
    IR::Value ReadVariableRecursive(auto variable, IR::Block* block) {
        IR::Value val;
        if (const std::span preds{block->ImmediatePredecessors()}; preds.size() == 1) {
            val = ReadVariable(variable, preds.front());
        } else {
            // Break potential cycles with operandless phi
            val = IR::Value{&*block->PrependNewInst(block->begin(), IR::Opcode::Phi)};
            WriteVariable(variable, block, val);
            val = AddPhiOperands(variable, val, block);
        }
        WriteVariable(variable, block, val);
        return val;
    }

    IR::Value AddPhiOperands(auto variable, const IR::Value& phi, IR::Block* block) {
        for (IR::Block* const pred : block->ImmediatePredecessors()) {
            phi.Inst()->AddPhiOperand(pred, ReadVariable(variable, pred));
        }
        return TryRemoveTrivialPhi(phi, block, UndefOpcode(variable));
    }

    IR::Value TryRemoveTrivialPhi(const IR::Value& phi, IR::Block* block, IR::Opcode undef_opcode) {
        IR::Value same;
        for (const auto& pair : phi.Inst()->PhiOperands()) {
            const IR::Value& op{pair.second};
            if (op == same || op == phi) {
                // Unique value or self-reference
                continue;
            }
            if (!same.IsEmpty()) {
                // The phi merges at least two values: not trivial
                return phi;
            }
            same = op;
        }
        if (same.IsEmpty()) {
            // The phi is unreachable or in the start block
            const auto first_not_phi{std::ranges::find_if_not(block->Instructions(), IsPhi)};
            same = IR::Value{&*block->PrependNewInst(first_not_phi, undef_opcode)};
        }
        // Reroute all uses of phi to same and remove phi
        phi.Inst()->ReplaceUsesWith(same);
        // TODO: Try to recursively remove all phi users, which might have become trivial
        return same;
    }

    DefTable current_def;
};
} // Anonymous namespace

void SsaRewritePass(IR::Function& function) {
    Pass pass;
    for (const auto& block : function.blocks) {
        for (IR::Inst& inst : block->Instructions()) {
            switch (inst.Opcode()) {
            case IR::Opcode::SetRegister:
                if (const IR::Reg reg{inst.Arg(0).Reg()}; reg != IR::Reg::RZ) {
                    pass.WriteVariable(reg, block.get(), inst.Arg(1));
                }
                break;
            case IR::Opcode::SetPred:
                if (const IR::Pred pred{inst.Arg(0).Pred()}; pred != IR::Pred::PT) {
                    pass.WriteVariable(pred, block.get(), inst.Arg(1));
                }
                break;
            case IR::Opcode::SetZFlag:
                pass.WriteVariable(ZeroFlagTag{}, block.get(), inst.Arg(0));
                break;
            case IR::Opcode::SetSFlag:
                pass.WriteVariable(SignFlagTag{}, block.get(), inst.Arg(0));
                break;
            case IR::Opcode::SetCFlag:
                pass.WriteVariable(CarryFlagTag{}, block.get(), inst.Arg(0));
                break;
            case IR::Opcode::SetOFlag:
                pass.WriteVariable(OverflowFlagTag{}, block.get(), inst.Arg(0));
                break;
            case IR::Opcode::GetRegister:
                if (const IR::Reg reg{inst.Arg(0).Reg()}; reg != IR::Reg::RZ) {
                    inst.ReplaceUsesWith(pass.ReadVariable(reg, block.get()));
                }
                break;
            case IR::Opcode::GetPred:
                if (const IR::Pred pred{inst.Arg(0).Pred()}; pred != IR::Pred::PT) {
                    inst.ReplaceUsesWith(pass.ReadVariable(pred, block.get()));
                }
                break;
            case IR::Opcode::GetZFlag:
                inst.ReplaceUsesWith(pass.ReadVariable(ZeroFlagTag{}, block.get()));
                break;
            case IR::Opcode::GetSFlag:
                inst.ReplaceUsesWith(pass.ReadVariable(SignFlagTag{}, block.get()));
                break;
            case IR::Opcode::GetCFlag:
                inst.ReplaceUsesWith(pass.ReadVariable(CarryFlagTag{}, block.get()));
                break;
            case IR::Opcode::GetOFlag:
                inst.ReplaceUsesWith(pass.ReadVariable(OverflowFlagTag{}, block.get()));
                break;
            default:
                break;
            }
        }
    }
}

} // namespace Shader::Optimization
