// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class Mode : u64 {
    PR,
    CC,
};
} // Anonymous namespace

void TranslatorVisitor::P2R_reg(u64) {
    throw NotImplementedException("P2R (reg)");
}

void TranslatorVisitor::P2R_cbuf(u64) {
    throw NotImplementedException("P2R (cbuf)");
}

void TranslatorVisitor::P2R_imm(u64 insn) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src;
        BitField<40, 1, Mode> mode;
        BitField<41, 2, u64> byte_selector;
    } const p2r{insn};

    const u32 mask{GetImm20(insn).U32()};
    const bool pr_mode{p2r.mode == Mode::PR};
    const u32 num_items{pr_mode ? 7U : 4U};
    const u32 offset{static_cast<u32>(p2r.byte_selector) * 8};
    IR::U32 insert{ir.Imm32(0)};
    for (u32 index = 0; index < num_items; ++index) {
        if (((mask >> index) & 1) == 0) {
            continue;
        }
        const IR::U1 cond{[this, index, pr_mode] {
            if (pr_mode) {
                return ir.GetPred(IR::Pred{index});
            }
            switch (index) {
            case 0:
                return ir.GetZFlag();
            case 1:
                return ir.GetSFlag();
            case 2:
                return ir.GetCFlag();
            case 3:
                return ir.GetOFlag();
            }
            throw LogicError("Unreachable P2R index");
        }()};
        const IR::U32 bit{ir.Select(cond, ir.Imm32(1U << (index + offset)), ir.Imm32(0))};
        insert = ir.BitwiseOr(insert, bit);
    }
    const IR::U32 masked_out{ir.BitwiseAnd(X(p2r.src), ir.Imm32(~(mask << offset)))};
    X(p2r.dest_reg, ir.BitwiseOr(masked_out, insert));
}

} // namespace Shader::Maxwell
