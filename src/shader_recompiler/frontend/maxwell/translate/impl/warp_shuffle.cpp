// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <optional>

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class ShuffleMode : u64 {
    IDX,
    UP,
    DOWN,
    BFLY,
};

[[nodiscard]] IR::U32 ShuffleOperation(IR::IREmitter& ir, const IR::U32& value,
                                       const IR::U32& index, const IR::U32& mask,
                                       ShuffleMode shfl_op) {
    const IR::U32 clamp{ir.BitFieldExtract(mask, ir.Imm32(0), ir.Imm32(5))};
    const IR::U32 seg_mask{ir.BitFieldExtract(mask, ir.Imm32(8), ir.Imm32(5))};
    switch (shfl_op) {
    case ShuffleMode::IDX:
        return ir.ShuffleIndex(value, index, clamp, seg_mask);
    case ShuffleMode::UP:
        return ir.ShuffleUp(value, index, clamp, seg_mask);
    case ShuffleMode::DOWN:
        return ir.ShuffleDown(value, index, clamp, seg_mask);
    case ShuffleMode::BFLY:
        return ir.ShuffleButterfly(value, index, clamp, seg_mask);
    default:
        throw NotImplementedException("Invalid SHFL op {}", shfl_op);
    }
}

void Shuffle(TranslatorVisitor& v, u64 insn, const IR::U32& index, const IR::U32& mask) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_reg;
        BitField<30, 2, ShuffleMode> mode;
        BitField<48, 3, IR::Pred> pred;
    } const shfl{insn};

    const IR::U32 result{ShuffleOperation(v.ir, v.X(shfl.src_reg), index, mask, shfl.mode)};
    v.ir.SetPred(shfl.pred, v.ir.GetInBoundsFromOp(result));
    v.X(shfl.dest_reg, result);
}
} // Anonymous namespace

void TranslatorVisitor::SHFL(u64 insn) {
    union {
        u64 insn;
        BitField<20, 5, u64> src_a_imm;
        BitField<28, 1, u64> src_a_flag;
        BitField<29, 1, u64> src_b_flag;
        BitField<34, 13, u64> src_b_imm;
    } const flags{insn};
    const IR::U32 src_a{flags.src_a_flag != 0 ? ir.Imm32(static_cast<u32>(flags.src_a_imm))
                                              : GetReg20(insn)};
    const IR::U32 src_b{flags.src_b_flag != 0 ? ir.Imm32(static_cast<u32>(flags.src_b_imm))
                                              : GetReg39(insn)};
    Shuffle(*this, insn, src_a, src_b);
}

} // namespace Shader::Maxwell
