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

void SetFlag(IR::IREmitter& ir, const IR::U1& inv_mask_bit, const IR::U1& src_bit, u32 index) {
    switch (index) {
    case 0:
        return ir.SetZFlag(IR::U1{ir.Select(inv_mask_bit, ir.GetZFlag(), src_bit)});
    case 1:
        return ir.SetSFlag(IR::U1{ir.Select(inv_mask_bit, ir.GetSFlag(), src_bit)});
    case 2:
        return ir.SetCFlag(IR::U1{ir.Select(inv_mask_bit, ir.GetCFlag(), src_bit)});
    case 3:
        return ir.SetOFlag(IR::U1{ir.Select(inv_mask_bit, ir.GetOFlag(), src_bit)});
    default:
        throw LogicError("Unreachable R2P index");
    }
}

void R2P(TranslatorVisitor& v, u64 insn, const IR::U32& mask) {
    union {
        u64 raw;
        BitField<8, 8, IR::Reg> src_reg;
        BitField<40, 1, Mode> mode;
        BitField<41, 2, u64> byte_selector;
    } const r2p{insn};
    const IR::U32 src{v.X(r2p.src_reg)};
    const IR::U32 count{v.ir.Imm32(1)};
    const bool pr_mode{r2p.mode == Mode::PR};
    const u32 num_items{pr_mode ? 7U : 4U};
    const u32 offset_base{static_cast<u32>(r2p.byte_selector) * 8};
    for (u32 index = 0; index < num_items; ++index) {
        const IR::U32 offset{v.ir.Imm32(offset_base + index)};
        const IR::U1 src_zero{v.ir.GetZeroFromOp(v.ir.BitFieldExtract(src, offset, count, false))};
        const IR::U1 src_bit{v.ir.LogicalNot(src_zero)};
        const IR::U32 mask_bfe{v.ir.BitFieldExtract(mask, v.ir.Imm32(index), count, false)};
        const IR::U1 inv_mask_bit{v.ir.GetZeroFromOp(mask_bfe)};
        if (pr_mode) {
            const IR::Pred pred{index};
            v.ir.SetPred(pred, IR::U1{v.ir.Select(inv_mask_bit, v.ir.GetPred(pred), src_bit)});
        } else {
            SetFlag(v.ir, inv_mask_bit, src_bit, index);
        }
    }
}
} // Anonymous namespace

void TranslatorVisitor::R2P_reg(u64 insn) {
    R2P(*this, insn, GetReg20(insn));
}

void TranslatorVisitor::R2P_cbuf(u64 insn) {
    R2P(*this, insn, GetCbuf(insn));
}

void TranslatorVisitor::R2P_imm(u64 insn) {
    R2P(*this, insn, GetImm20(insn));
}

} // namespace Shader::Maxwell
