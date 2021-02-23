// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class SelectMode : u64 {
    Default,
    CLO,
    CHI,
    CSFU,
    CBCC,
};

enum class Half : u64 {
    H0, // Least-significant bits (15:0)
    H1, // Most-significant bits (31:16)
};

IR::U32 ExtractHalf(TranslatorVisitor& v, const IR::U32& src, Half half, bool is_signed) {
    const IR::U32 offset{v.ir.Imm32(half == Half::H1 ? 16 : 0)};
    return v.ir.BitFieldExtract(src, offset, v.ir.Imm32(16), is_signed);
}

void XMAD(TranslatorVisitor& v, u64 insn, const IR::U32& src_b, const IR::U32& src_c,
          SelectMode select_mode, Half half_b, bool psl, bool mrg, bool x) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_reg_a;
        BitField<47, 1, u64> cc;
        BitField<48, 1, u64> is_a_signed;
        BitField<49, 1, u64> is_b_signed;
        BitField<53, 1, Half> half_a;
    } const xmad{insn};

    if (x) {
        throw NotImplementedException("XMAD X");
    }
    const IR::U32 op_a{ExtractHalf(v, v.X(xmad.src_reg_a), xmad.half_a, xmad.is_a_signed != 0)};
    const IR::U32 op_b{ExtractHalf(v, src_b, half_b, xmad.is_b_signed != 0)};

    IR::U32 product{v.ir.IMul(op_a, op_b)};
    if (psl) {
        // .PSL shifts the product 16 bits
        product = v.ir.ShiftLeftLogical(product, v.ir.Imm32(16));
    }
    const IR::U32 op_c{[&]() -> IR::U32 {
        switch (select_mode) {
        case SelectMode::Default:
            return src_c;
        case SelectMode::CLO:
            return ExtractHalf(v, src_c, Half::H0, false);
        case SelectMode::CHI:
            return ExtractHalf(v, src_c, Half::H1, false);
        case SelectMode::CBCC:
            return v.ir.IAdd(v.ir.ShiftLeftLogical(src_b, v.ir.Imm32(16)), src_c);
        case SelectMode::CSFU:
            throw NotImplementedException("XMAD CSFU");
        }
        throw NotImplementedException("Invalid XMAD select mode {}", select_mode);
    }()};
    IR::U32 result{v.ir.IAdd(product, op_c)};
    if (mrg) {
        // .MRG inserts src_b [15:0] into result's [31:16].
        const IR::U32 lsb_b{ExtractHalf(v, src_b, Half::H0, false)};
        result = v.ir.BitFieldInsert(result, lsb_b, v.ir.Imm32(16), v.ir.Imm32(16));
    }
    if (xmad.cc) {
        throw NotImplementedException("XMAD CC");
    }
    // Store result
    v.X(xmad.dest_reg, result);
}
} // Anonymous namespace

void TranslatorVisitor::XMAD_reg(u64 insn) {
    union {
        u64 raw;
        BitField<35, 1, Half> half_b;
        BitField<36, 1, u64> psl;
        BitField<37, 1, u64> mrg;
        BitField<38, 1, u64> x;
        BitField<50, 3, SelectMode> select_mode;
    } const xmad{insn};

    XMAD(*this, insn, GetReg20(insn), GetReg39(insn), xmad.select_mode, xmad.half_b, xmad.psl != 0,
         xmad.mrg != 0, xmad.x != 0);
}

void TranslatorVisitor::XMAD_rc(u64 insn) {
    union {
        u64 raw;
        BitField<50, 2, SelectMode> select_mode;
        BitField<52, 1, Half> half_b;
        BitField<54, 1, u64> x;
    } const xmad{insn};

    XMAD(*this, insn, GetReg39(insn), GetCbuf(insn), xmad.select_mode, xmad.half_b, false, false,
         xmad.x != 0);
}

void TranslatorVisitor::XMAD_cr(u64 insn) {
    union {
        u64 raw;
        BitField<50, 2, SelectMode> select_mode;
        BitField<52, 1, Half> half_b;
        BitField<54, 1, u64> x;
        BitField<55, 1, u64> psl;
        BitField<56, 1, u64> mrg;
    } const xmad{insn};

    XMAD(*this, insn, GetCbuf(insn), GetReg39(insn), xmad.select_mode, xmad.half_b, xmad.psl != 0,
         xmad.mrg != 0, xmad.x != 0);
}

void TranslatorVisitor::XMAD_imm(u64 insn) {
    union {
        u64 raw;
        BitField<20, 16, u64> src_b;
        BitField<36, 1, u64> psl;
        BitField<37, 1, u64> mrg;
        BitField<38, 1, u64> x;
        BitField<50, 3, SelectMode> select_mode;
    } const xmad{insn};

    XMAD(*this, insn, ir.Imm32(static_cast<u32>(xmad.src_b)), GetReg39(insn), xmad.select_mode,
         Half::H0, xmad.psl != 0, xmad.mrg != 0, xmad.x != 0);
}

} // namespace Shader::Maxwell
