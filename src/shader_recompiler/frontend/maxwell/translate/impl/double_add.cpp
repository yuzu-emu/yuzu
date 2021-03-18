// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/common_encoding.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {

void DADD(TranslatorVisitor& v, u64 insn, const IR::F64& src_b) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_a_reg;
        BitField<39, 2, FpRounding> fp_rounding;
        BitField<45, 1, u64> neg_b;
        BitField<46, 1, u64> abs_a;
        BitField<47, 1, u64> cc;
        BitField<48, 1, u64> neg_a;
        BitField<49, 1, u64> abs_b;
    } const dadd{insn};

    if (!IR::IsAligned(dadd.dest_reg, 2)) {
        throw NotImplementedException("Unaligned destination register {}", dadd.dest_reg.Value());
    }
    if (!IR::IsAligned(dadd.src_a_reg, 2)) {
        throw NotImplementedException("Unaligned destination register {}", dadd.src_a_reg.Value());
    }
    if (dadd.cc != 0) {
        throw NotImplementedException("DADD CC");
    }

    const IR::Reg reg_a{dadd.src_a_reg};
    const IR::F64 src_a{v.ir.PackDouble2x32(v.ir.CompositeConstruct(v.X(reg_a), v.X(reg_a + 1)))};
    const IR::F64 op_a{v.ir.FPAbsNeg(src_a, dadd.abs_a != 0, dadd.neg_a != 0)};
    const IR::F64 op_b{v.ir.FPAbsNeg(src_b, dadd.abs_b != 0, dadd.neg_b != 0)};

    IR::FpControl control{
        .no_contraction{true},
        .rounding{CastFpRounding(dadd.fp_rounding)},
        .fmz_mode{IR::FmzMode::None},
    };
    const IR::F64 value{v.ir.FPAdd(op_a, op_b, control)};
    const IR::Value result{v.ir.UnpackDouble2x32(value)};

    for (int i = 0; i < 2; i++) {
        v.X(dadd.dest_reg + i, IR::U32{v.ir.CompositeExtract(result, i)});
    }
}
} // Anonymous namespace

void TranslatorVisitor::DADD_reg(u64 insn) {
    DADD(*this, insn, GetDoubleReg20(insn));
}

void TranslatorVisitor::DADD_cbuf(u64 insn) {
    DADD(*this, insn, GetDoubleCbuf(insn));
}

void TranslatorVisitor::DADD_imm(u64 insn) {
    DADD(*this, insn, GetDoubleImm20(insn));
}

} // namespace Shader::Maxwell
