// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <type_traits>

#include "common/bit_cast.h"
#include "common/bit_util.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/ir/microinstruction.h"
#include "shader_recompiler/ir_opt/passes.h"

namespace Shader::Optimization {
namespace {
[[nodiscard]] u32 BitFieldUExtract(u32 base, u32 shift, u32 count) {
    if (static_cast<size_t>(shift) + static_cast<size_t>(count) > Common::BitSize<u32>()) {
        throw LogicError("Undefined result in BitFieldUExtract({}, {}, {})", base, shift, count);
    }
    return (base >> shift) & ((1U << count) - 1);
}

template <typename T>
[[nodiscard]] T Arg(const IR::Value& value) {
    if constexpr (std::is_same_v<T, bool>) {
        return value.U1();
    } else if constexpr (std::is_same_v<T, u32>) {
        return value.U32();
    } else if constexpr (std::is_same_v<T, f32>) {
        return value.F32();
    } else if constexpr (std::is_same_v<T, u64>) {
        return value.U64();
    }
}

template <typename ImmFn>
bool FoldCommutative(IR::Inst& inst, ImmFn&& imm_fn) {
    const auto arg = [](const IR::Value& value) {
        if constexpr (std::is_invocable_r_v<bool, ImmFn, bool, bool>) {
            return value.U1();
        } else if constexpr (std::is_invocable_r_v<u32, ImmFn, u32, u32>) {
            return value.U32();
        } else if constexpr (std::is_invocable_r_v<u64, ImmFn, u64, u64>) {
            return value.U64();
        }
    };
    const IR::Value lhs{inst.Arg(0)};
    const IR::Value rhs{inst.Arg(1)};

    const bool is_lhs_immediate{lhs.IsImmediate()};
    const bool is_rhs_immediate{rhs.IsImmediate()};

    if (is_lhs_immediate && is_rhs_immediate) {
        const auto result{imm_fn(arg(lhs), arg(rhs))};
        inst.ReplaceUsesWith(IR::Value{result});
        return false;
    }
    if (is_lhs_immediate && !is_rhs_immediate) {
        IR::Inst* const rhs_inst{rhs.InstRecursive()};
        if (rhs_inst->Opcode() == inst.Opcode() && rhs_inst->Arg(1).IsImmediate()) {
            const auto combined{imm_fn(arg(lhs), arg(rhs_inst->Arg(1)))};
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
            const auto combined{imm_fn(arg(rhs), arg(lhs_inst->Arg(1)))};
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
    if (!FoldCommutative(inst, [](T a, T b) { return a + b; })) {
        return;
    }
    const IR::Value rhs{inst.Arg(1)};
    if (rhs.IsImmediate() && Arg<T>(rhs) == 0) {
        inst.ReplaceUsesWith(inst.Arg(0));
    }
}

void FoldLogicalAnd(IR::Inst& inst) {
    if (!FoldCommutative(inst, [](bool a, bool b) { return a && b; })) {
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
    case IR::Opcode::BitFieldUExtract:
        if (inst.AreAllArgsImmediates() && !inst.HasAssociatedPseudoOperation()) {
            inst.ReplaceUsesWith(IR::Value{
                BitFieldUExtract(inst.Arg(0).U32(), inst.Arg(1).U32(), inst.Arg(2).U32())});
        }
        break;
    case IR::Opcode::LogicalAnd:
        return FoldLogicalAnd(inst);
    default:
        break;
    }
}
} // Anonymous namespace

void ConstantPropagationPass(IR::Block& block) {
    std::ranges::for_each(block, ConstantPropagation);
}

} // namespace Shader::Optimization
