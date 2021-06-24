// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <ranges>
#include <utility>

#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/ir_opt/passes.h"

namespace Shader::Optimization {
namespace {
std::pair<IR::U32, IR::U32> Unpack(IR::IREmitter& ir, const IR::Value& packed) {
    if (packed.IsImmediate()) {
        const u64 value{packed.U64()};
        return {
            ir.Imm32(static_cast<u32>(value)),
            ir.Imm32(static_cast<u32>(value >> 32)),
        };
    } else {
        return std::pair<IR::U32, IR::U32>{
            ir.CompositeExtract(packed, 0u),
            ir.CompositeExtract(packed, 1u),
        };
    }
}

void IAdd64To32(IR::Block& block, IR::Inst& inst) {
    if (inst.HasAssociatedPseudoOperation()) {
        throw NotImplementedException("IAdd64 emulation with pseudo instructions");
    }
    IR::IREmitter ir(block, IR::Block::InstructionList::s_iterator_to(inst));
    const auto [a_lo, a_hi]{Unpack(ir, inst.Arg(0))};
    const auto [b_lo, b_hi]{Unpack(ir, inst.Arg(1))};

    const IR::U32 ret_lo{ir.IAdd(a_lo, b_lo)};
    const IR::U32 carry{ir.Select(ir.GetCarryFromOp(ret_lo), ir.Imm32(1u), ir.Imm32(0u))};

    const IR::U32 ret_hi{ir.IAdd(ir.IAdd(a_hi, b_hi), carry)};
    inst.ReplaceUsesWith(ir.CompositeConstruct(ret_lo, ret_hi));
}

void ISub64To32(IR::Block& block, IR::Inst& inst) {
    if (inst.HasAssociatedPseudoOperation()) {
        throw NotImplementedException("ISub64 emulation with pseudo instructions");
    }
    IR::IREmitter ir(block, IR::Block::InstructionList::s_iterator_to(inst));
    const auto [a_lo, a_hi]{Unpack(ir, inst.Arg(0))};
    const auto [b_lo, b_hi]{Unpack(ir, inst.Arg(1))};

    const IR::U32 ret_lo{ir.ISub(a_lo, b_lo)};
    const IR::U1 underflow{ir.IGreaterThan(ret_lo, a_lo, false)};
    const IR::U32 underflow_bit{ir.Select(underflow, ir.Imm32(1u), ir.Imm32(0u))};

    const IR::U32 ret_hi{ir.ISub(ir.ISub(a_hi, b_hi), underflow_bit)};
    inst.ReplaceUsesWith(ir.CompositeConstruct(ret_lo, ret_hi));
}

void INeg64To32(IR::Block& block, IR::Inst& inst) {
    if (inst.HasAssociatedPseudoOperation()) {
        throw NotImplementedException("INeg64 emulation with pseudo instructions");
    }
    IR::IREmitter ir(block, IR::Block::InstructionList::s_iterator_to(inst));
    auto [lo, hi]{Unpack(ir, inst.Arg(0))};
    lo = ir.BitwiseNot(lo);
    hi = ir.BitwiseNot(hi);

    lo = ir.IAdd(lo, ir.Imm32(1));

    const IR::U32 carry{ir.Select(ir.GetCarryFromOp(lo), ir.Imm32(1u), ir.Imm32(0u))};
    hi = ir.IAdd(hi, carry);

    inst.ReplaceUsesWith(ir.CompositeConstruct(lo, hi));
}

void ShiftLeftLogical64To32(IR::Block& block, IR::Inst& inst) {
    if (inst.HasAssociatedPseudoOperation()) {
        throw NotImplementedException("ShiftLeftLogical64 emulation with pseudo instructions");
    }
    IR::IREmitter ir(block, IR::Block::InstructionList::s_iterator_to(inst));
    const auto [lo, hi]{Unpack(ir, inst.Arg(0))};
    const IR::U32 shift{inst.Arg(1)};

    const IR::U32 shifted_lo{ir.ShiftLeftLogical(lo, shift)};
    const IR::U32 shifted_hi{ir.ShiftLeftLogical(hi, shift)};

    const IR::U32 inv_shift{ir.ISub(shift, ir.Imm32(32))};
    const IR::U1 is_long{ir.IGreaterThanEqual(inv_shift, ir.Imm32(0), true)};
    const IR::U1 is_zero{ir.IEqual(shift, ir.Imm32(0))};

    const IR::U32 long_ret_lo{ir.Imm32(0)};
    const IR::U32 long_ret_hi{ir.ShiftLeftLogical(lo, inv_shift)};

    const IR::U32 shift_complement{ir.ISub(ir.Imm32(32), shift)};
    const IR::U32 lo_extract{ir.BitFieldExtract(lo, shift_complement, shift, false)};
    const IR::U32 short_ret_lo{shifted_lo};
    const IR::U32 short_ret_hi{ir.BitwiseOr(shifted_hi, lo_extract)};

    const IR::U32 zero_ret_lo{lo};
    const IR::U32 zero_ret_hi{hi};

    const IR::U32 non_zero_lo{ir.Select(is_long, long_ret_lo, short_ret_lo)};
    const IR::U32 non_zero_hi{ir.Select(is_long, long_ret_hi, short_ret_hi)};

    const IR::U32 ret_lo{ir.Select(is_zero, zero_ret_lo, non_zero_lo)};
    const IR::U32 ret_hi{ir.Select(is_zero, zero_ret_hi, non_zero_hi)};
    inst.ReplaceUsesWith(ir.CompositeConstruct(ret_lo, ret_hi));
}

void ShiftRightLogical64To32(IR::Block& block, IR::Inst& inst) {
    if (inst.HasAssociatedPseudoOperation()) {
        throw NotImplementedException("ShiftRightLogical64 emulation with pseudo instructions");
    }
    IR::IREmitter ir(block, IR::Block::InstructionList::s_iterator_to(inst));
    const auto [lo, hi]{Unpack(ir, inst.Arg(0))};
    const IR::U32 shift{inst.Arg(1)};

    const IR::U32 shifted_lo{ir.ShiftRightLogical(lo, shift)};
    const IR::U32 shifted_hi{ir.ShiftRightLogical(hi, shift)};

    const IR::U32 inv_shift{ir.ISub(shift, ir.Imm32(32))};
    const IR::U1 is_long{ir.IGreaterThanEqual(inv_shift, ir.Imm32(0), true)};
    const IR::U1 is_zero{ir.IEqual(shift, ir.Imm32(0))};

    const IR::U32 long_ret_hi{ir.Imm32(0)};
    const IR::U32 long_ret_lo{ir.ShiftRightLogical(hi, inv_shift)};

    const IR::U32 shift_complement{ir.ISub(ir.Imm32(32), shift)};
    const IR::U32 short_hi_extract{ir.BitFieldExtract(hi, ir.Imm32(0), shift)};
    const IR::U32 short_ret_hi{shifted_hi};
    const IR::U32 short_ret_lo{
        ir.BitFieldInsert(shifted_lo, short_hi_extract, shift_complement, shift)};

    const IR::U32 zero_ret_lo{lo};
    const IR::U32 zero_ret_hi{hi};

    const IR::U32 non_zero_lo{ir.Select(is_long, long_ret_lo, short_ret_lo)};
    const IR::U32 non_zero_hi{ir.Select(is_long, long_ret_hi, short_ret_hi)};

    const IR::U32 ret_lo{ir.Select(is_zero, zero_ret_lo, non_zero_lo)};
    const IR::U32 ret_hi{ir.Select(is_zero, zero_ret_hi, non_zero_hi)};
    inst.ReplaceUsesWith(ir.CompositeConstruct(ret_lo, ret_hi));
}

void ShiftRightArithmetic64To32(IR::Block& block, IR::Inst& inst) {
    if (inst.HasAssociatedPseudoOperation()) {
        throw NotImplementedException("ShiftRightArithmetic64 emulation with pseudo instructions");
    }
    IR::IREmitter ir(block, IR::Block::InstructionList::s_iterator_to(inst));
    const auto [lo, hi]{Unpack(ir, inst.Arg(0))};
    const IR::U32 shift{inst.Arg(1)};

    const IR::U32 shifted_lo{ir.ShiftRightLogical(lo, shift)};
    const IR::U32 shifted_hi{ir.ShiftRightArithmetic(hi, shift)};

    const IR::U32 sign_extension{ir.ShiftRightArithmetic(hi, ir.Imm32(31))};

    const IR::U32 inv_shift{ir.ISub(shift, ir.Imm32(32))};
    const IR::U1 is_long{ir.IGreaterThanEqual(inv_shift, ir.Imm32(0), true)};
    const IR::U1 is_zero{ir.IEqual(shift, ir.Imm32(0))};

    const IR::U32 long_ret_hi{sign_extension};
    const IR::U32 long_ret_lo{ir.ShiftRightArithmetic(hi, inv_shift)};

    const IR::U32 shift_complement{ir.ISub(ir.Imm32(32), shift)};
    const IR::U32 short_hi_extract(ir.BitFieldExtract(hi, ir.Imm32(0), shift));
    const IR::U32 short_ret_hi{shifted_hi};
    const IR::U32 short_ret_lo{
        ir.BitFieldInsert(shifted_lo, short_hi_extract, shift_complement, shift)};

    const IR::U32 zero_ret_lo{lo};
    const IR::U32 zero_ret_hi{hi};

    const IR::U32 non_zero_lo{ir.Select(is_long, long_ret_lo, short_ret_lo)};
    const IR::U32 non_zero_hi{ir.Select(is_long, long_ret_hi, short_ret_hi)};

    const IR::U32 ret_lo{ir.Select(is_zero, zero_ret_lo, non_zero_lo)};
    const IR::U32 ret_hi{ir.Select(is_zero, zero_ret_hi, non_zero_hi)};
    inst.ReplaceUsesWith(ir.CompositeConstruct(ret_lo, ret_hi));
}

void Lower(IR::Block& block, IR::Inst& inst) {
    switch (inst.GetOpcode()) {
    case IR::Opcode::PackUint2x32:
    case IR::Opcode::UnpackUint2x32:
        return inst.ReplaceOpcode(IR::Opcode::Identity);
    case IR::Opcode::IAdd64:
        return IAdd64To32(block, inst);
    case IR::Opcode::ISub64:
        return ISub64To32(block, inst);
    case IR::Opcode::INeg64:
        return INeg64To32(block, inst);
    case IR::Opcode::ShiftLeftLogical64:
        return ShiftLeftLogical64To32(block, inst);
    case IR::Opcode::ShiftRightLogical64:
        return ShiftRightLogical64To32(block, inst);
    case IR::Opcode::ShiftRightArithmetic64:
        return ShiftRightArithmetic64To32(block, inst);
    default:
        break;
    }
}
} // Anonymous namespace

void LowerInt64ToInt32(IR::Program& program) {
    for (IR::Block* const block : program.post_order_blocks | std::views::reverse) {
        for (IR::Inst& inst : block->Instructions()) {
            Lower(*block, inst);
        }
    }
}

} // namespace Shader::Optimization
