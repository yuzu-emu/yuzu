// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <tuple>
#include <type_traits>

#include "common/bit_cast.h"
#include "common/bit_util.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/ir/microinstruction.h"
#include "shader_recompiler/ir_opt/passes.h"

namespace Shader::Optimization {
namespace {
// Metaprogramming stuff to get arguments information out of a lambda
template <typename Func>
struct LambdaTraits : LambdaTraits<decltype(&std::remove_reference_t<Func>::operator())> {};

template <typename ReturnType, typename LambdaType, typename... Args>
struct LambdaTraits<ReturnType (LambdaType::*)(Args...) const> {
    template <size_t I>
    using ArgType = std::tuple_element_t<I, std::tuple<Args...>>;

    static constexpr size_t NUM_ARGS{sizeof...(Args)};
};

template <typename T>
[[nodiscard]] T Arg(const IR::Value& value) {
    if constexpr (std::is_same_v<T, bool>) {
        return value.U1();
    } else if constexpr (std::is_same_v<T, u32>) {
        return value.U32();
    } else if constexpr (std::is_same_v<T, s32>) {
        return static_cast<s32>(value.U32());
    } else if constexpr (std::is_same_v<T, f32>) {
        return value.F32();
    } else if constexpr (std::is_same_v<T, u64>) {
        return value.U64();
    }
}

template <typename T, typename ImmFn>
bool FoldCommutative(IR::Inst& inst, ImmFn&& imm_fn) {
    const IR::Value lhs{inst.Arg(0)};
    const IR::Value rhs{inst.Arg(1)};

    const bool is_lhs_immediate{lhs.IsImmediate()};
    const bool is_rhs_immediate{rhs.IsImmediate()};

    if (is_lhs_immediate && is_rhs_immediate) {
        const auto result{imm_fn(Arg<T>(lhs), Arg<T>(rhs))};
        inst.ReplaceUsesWith(IR::Value{result});
        return false;
    }
    if (is_lhs_immediate && !is_rhs_immediate) {
        IR::Inst* const rhs_inst{rhs.InstRecursive()};
        if (rhs_inst->Opcode() == inst.Opcode() && rhs_inst->Arg(1).IsImmediate()) {
            const auto combined{imm_fn(Arg<T>(lhs), Arg<T>(rhs_inst->Arg(1)))};
            inst.SetArg(0, rhs_inst->Arg(0));
            inst.SetArg(1, IR::Value{combined});
        } else {
            // Normalize
            inst.SetArg(0, rhs);
            inst.SetArg(1, lhs);
        }
    }
    if (!is_lhs_immediate && is_rhs_immediate) {
        const IR::Inst* const lhs_inst{lhs.InstRecursive()};
        if (lhs_inst->Opcode() == inst.Opcode() && lhs_inst->Arg(1).IsImmediate()) {
            const auto combined{imm_fn(Arg<T>(rhs), Arg<T>(lhs_inst->Arg(1)))};
            inst.SetArg(0, lhs_inst->Arg(0));
            inst.SetArg(1, IR::Value{combined});
        }
    }
    return true;
}

void FoldGetRegister(IR::Inst& inst) {
    if (inst.Arg(0).Reg() == IR::Reg::RZ) {
        inst.ReplaceUsesWith(IR::Value{u32{0}});
    }
}

void FoldGetPred(IR::Inst& inst) {
    if (inst.Arg(0).Pred() == IR::Pred::PT) {
        inst.ReplaceUsesWith(IR::Value{true});
    }
}

template <typename T>
void FoldAdd(IR::Inst& inst) {
    if (inst.HasAssociatedPseudoOperation()) {
        return;
    }
    if (!FoldCommutative<T>(inst, [](T a, T b) { return a + b; })) {
        return;
    }
    const IR::Value rhs{inst.Arg(1)};
    if (rhs.IsImmediate() && Arg<T>(rhs) == 0) {
        inst.ReplaceUsesWith(inst.Arg(0));
    }
}

template <typename T>
void FoldSelect(IR::Inst& inst) {
    const IR::Value cond{inst.Arg(0)};
    if (cond.IsImmediate()) {
        inst.ReplaceUsesWith(cond.U1() ? inst.Arg(1) : inst.Arg(2));
    }
}

void FoldLogicalAnd(IR::Inst& inst) {
    if (!FoldCommutative<bool>(inst, [](bool a, bool b) { return a && b; })) {
        return;
    }
    const IR::Value rhs{inst.Arg(1)};
    if (rhs.IsImmediate()) {
        if (rhs.U1()) {
            inst.ReplaceUsesWith(inst.Arg(0));
        } else {
            inst.ReplaceUsesWith(IR::Value{false});
        }
    }
}

void FoldLogicalOr(IR::Inst& inst) {
    if (!FoldCommutative<bool>(inst, [](bool a, bool b) { return a || b; })) {
        return;
    }
    const IR::Value rhs{inst.Arg(1)};
    if (rhs.IsImmediate()) {
        if (rhs.U1()) {
            inst.ReplaceUsesWith(IR::Value{true});
        } else {
            inst.ReplaceUsesWith(inst.Arg(0));
        }
    }
}

void FoldLogicalNot(IR::Inst& inst) {
    const IR::U1 value{inst.Arg(0)};
    if (value.IsImmediate()) {
        inst.ReplaceUsesWith(IR::Value{!value.U1()});
        return;
    }
    IR::Inst* const arg{value.InstRecursive()};
    if (arg->Opcode() == IR::Opcode::LogicalNot) {
        inst.ReplaceUsesWith(arg->Arg(0));
    }
}

template <typename Dest, typename Source>
void FoldBitCast(IR::Inst& inst, IR::Opcode reverse) {
    const IR::Value value{inst.Arg(0)};
    if (value.IsImmediate()) {
        inst.ReplaceUsesWith(IR::Value{Common::BitCast<Dest>(Arg<Source>(value))});
        return;
    }
    IR::Inst* const arg_inst{value.InstRecursive()};
    if (value.InstRecursive()->Opcode() == reverse) {
        inst.ReplaceUsesWith(arg_inst->Arg(0));
    }
}

template <typename Func, size_t... I>
IR::Value EvalImmediates(const IR::Inst& inst, Func&& func, std::index_sequence<I...>) {
    using Traits = LambdaTraits<decltype(func)>;
    return IR::Value{func(Arg<Traits::ArgType<I>>(inst.Arg(I))...)};
}

template <typename Func>
void FoldWhenAllImmediates(IR::Inst& inst, Func&& func) {
    if (!inst.AreAllArgsImmediates() || inst.HasAssociatedPseudoOperation()) {
        return;
    }
    using Indices = std::make_index_sequence<LambdaTraits<decltype(func)>::NUM_ARGS>;
    inst.ReplaceUsesWith(EvalImmediates(inst, func, Indices{}));
}

void FoldBranchConditional(IR::Inst& inst) {
    const IR::U1 cond{inst.Arg(0)};
    if (cond.IsImmediate()) {
        // TODO: Convert to Branch
        return;
    }
    const IR::Inst* cond_inst{cond.InstRecursive()};
    if (cond_inst->Opcode() == IR::Opcode::LogicalNot) {
        const IR::Value true_label{inst.Arg(1)};
        const IR::Value false_label{inst.Arg(2)};
        // Remove negation on the conditional (take the parameter out of LogicalNot) and swap
        // the branches
        inst.SetArg(0, cond_inst->Arg(0));
        inst.SetArg(1, false_label);
        inst.SetArg(2, true_label);
    }
}

void ConstantPropagation(IR::Inst& inst) {
    switch (inst.Opcode()) {
    case IR::Opcode::GetRegister:
        return FoldGetRegister(inst);
    case IR::Opcode::GetPred:
        return FoldGetPred(inst);
    case IR::Opcode::IAdd32:
        return FoldAdd<u32>(inst);
    case IR::Opcode::BitCastF32U32:
        return FoldBitCast<f32, u32>(inst, IR::Opcode::BitCastU32F32);
    case IR::Opcode::BitCastU32F32:
        return FoldBitCast<u32, f32>(inst, IR::Opcode::BitCastF32U32);
    case IR::Opcode::IAdd64:
        return FoldAdd<u64>(inst);
    case IR::Opcode::Select32:
        return FoldSelect<u32>(inst);
    case IR::Opcode::LogicalAnd:
        return FoldLogicalAnd(inst);
    case IR::Opcode::LogicalOr:
        return FoldLogicalOr(inst);
    case IR::Opcode::LogicalNot:
        return FoldLogicalNot(inst);
    case IR::Opcode::SLessThan:
        return FoldWhenAllImmediates(inst, [](s32 a, s32 b) { return a < b; });
    case IR::Opcode::ULessThan:
        return FoldWhenAllImmediates(inst, [](u32 a, u32 b) { return a < b; });
    case IR::Opcode::BitFieldUExtract:
        return FoldWhenAllImmediates(inst, [](u32 base, u32 shift, u32 count) {
            if (static_cast<size_t>(shift) + static_cast<size_t>(count) > Common::BitSize<u32>()) {
                throw LogicError("Undefined result in {}({}, {}, {})", IR::Opcode::BitFieldUExtract,
                                 base, shift, count);
            }
            return (base >> shift) & ((1U << count) - 1);
        });
    case IR::Opcode::BranchConditional:
        return FoldBranchConditional(inst);
    default:
        break;
    }
}
} // Anonymous namespace

void ConstantPropagationPass(IR::Block& block) {
    std::ranges::for_each(block, ConstantPropagation);
}

} // namespace Shader::Optimization
