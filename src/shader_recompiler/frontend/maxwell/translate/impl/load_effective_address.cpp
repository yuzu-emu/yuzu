// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
void LEA_hi(TranslatorVisitor& v, u64 insn, const IR::U32& base, IR::U32 offset_hi, u64 scale,
            bool neg, bool x) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> offset_lo_reg;
        BitField<47, 1, u64> cc;
        BitField<48, 3, IR::Pred> pred;
    } const lea{insn};

    if (x) {
        throw NotImplementedException("LEA.HI X");
    }
    if (lea.pred != IR::Pred::PT) {
        throw NotImplementedException("LEA.HI Pred");
    }
    if (lea.cc != 0) {
        throw NotImplementedException("LEA.HI CC");
    }

    const IR::U32 offset_lo{v.X(lea.offset_lo_reg)};
    const IR::U64 packed_offset{v.ir.PackUint2x32(v.ir.CompositeConstruct(offset_lo, offset_hi))};
    const IR::U64 offset{neg ? IR::U64{v.ir.INeg(packed_offset)} : packed_offset};

    const s32 hi_scale{32 - static_cast<s32>(scale)};
    const IR::U64 scaled_offset{v.ir.ShiftRightLogical(offset, v.ir.Imm32(hi_scale))};
    const IR::U32 scaled_offset_w0{v.ir.CompositeExtract(v.ir.UnpackUint2x32(scaled_offset), 0)};

    IR::U32 result{v.ir.IAdd(base, scaled_offset_w0)};
    v.X(lea.dest_reg, result);
}

void LEA_lo(TranslatorVisitor& v, u64 insn, const IR::U32& base) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> offset_lo_reg;
        BitField<39, 5, u64> scale;
        BitField<45, 1, u64> neg;
        BitField<46, 1, u64> x;
        BitField<47, 1, u64> cc;
        BitField<48, 3, IR::Pred> pred;
    } const lea{insn};
    if (lea.x != 0) {
        throw NotImplementedException("LEA.LO X");
    }
    if (lea.pred != IR::Pred::PT) {
        throw NotImplementedException("LEA.LO Pred");
    }
    if (lea.cc != 0) {
        throw NotImplementedException("LEA.LO CC");
    }

    const IR::U32 offset_lo{v.X(lea.offset_lo_reg)};
    const s32 scale{static_cast<s32>(lea.scale)};
    const IR::U32 offset{lea.neg != 0 ? IR::U32{v.ir.INeg(offset_lo)} : offset_lo};
    const IR::U32 scaled_offset{v.ir.ShiftLeftLogical(offset, v.ir.Imm32(scale))};

    IR::U32 result{v.ir.IAdd(base, scaled_offset)};
    v.X(lea.dest_reg, result);
}
} // Anonymous namespace

void TranslatorVisitor::LEA_hi_reg(u64 insn) {
    union {
        u64 insn;
        BitField<28, 5, u64> scale;
        BitField<37, 1, u64> neg;
        BitField<38, 1, u64> x;
    } const lea{insn};

    LEA_hi(*this, insn, GetReg20(insn), GetReg39(insn), lea.scale, lea.neg != 0, lea.x != 0);
}

void TranslatorVisitor::LEA_hi_cbuf(u64 insn) {
    union {
        u64 insn;
        BitField<51, 5, u64> scale;
        BitField<56, 1, u64> neg;
        BitField<57, 1, u64> x;
    } const lea{insn};

    LEA_hi(*this, insn, GetCbuf(insn), GetReg39(insn), lea.scale, lea.neg != 0, lea.x != 0);
}

void TranslatorVisitor::LEA_lo_reg(u64 insn) {
    LEA_lo(*this, insn, GetReg20(insn));
}

void TranslatorVisitor::LEA_lo_cbuf(u64 insn) {
    LEA_lo(*this, insn, GetCbuf(insn));
}

void TranslatorVisitor::LEA_lo_imm(u64 insn) {
    LEA_lo(*this, insn, GetImm20(insn));
}

} // namespace Shader::Maxwell
