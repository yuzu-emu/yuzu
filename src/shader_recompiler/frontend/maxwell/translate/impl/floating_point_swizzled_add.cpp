// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/common_encoding.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
void TranslatorVisitor::FSWZADD(u64 insn) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<28, 8, u64> swizzle;
        BitField<38, 1, u64> ndv;
        BitField<39, 2, FpRounding> round;
        BitField<44, 1, u64> ftz;
        BitField<47, 1, u64> cc;
    } const fswzadd{insn};

    if (fswzadd.ndv != 0) {
        throw NotImplementedException("FSWZADD NDV");
    }

    const IR::F32 src_a{GetFloatReg8(insn)};
    const IR::F32 src_b{GetFloatReg20(insn)};
    const IR::U32 swizzle{ir.Imm32(static_cast<u32>(fswzadd.swizzle))};

    const IR::FpControl fp_control{
        .no_contraction = false,
        .rounding = CastFpRounding(fswzadd.round),
        .fmz_mode = (fswzadd.ftz != 0 ? IR::FmzMode::FTZ : IR::FmzMode::None),
    };

    const IR::F32 result{ir.FSwizzleAdd(src_a, src_b, swizzle, fp_control)};
    F(fswzadd.dest_reg, result);

    if (fswzadd.cc != 0) {
        throw NotImplementedException("FSWZADD CC");
    }
}

} // namespace Shader::Maxwell
