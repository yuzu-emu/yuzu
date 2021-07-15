// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/common_encoding.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class FloatFormat : u64 {
    F16 = 1,
    F32 = 2,
    F64 = 3,
};

enum class IntFormat : u64 {
    U8 = 0,
    U16 = 1,
    U32 = 2,
    U64 = 3,
};

union Encoding {
    u64 raw;
    BitField<0, 8, IR::Reg> dest_reg;
    BitField<8, 2, FloatFormat> float_format;
    BitField<10, 2, IntFormat> int_format;
    BitField<13, 1, u64> is_signed;
    BitField<39, 2, FpRounding> fp_rounding;
    BitField<41, 2, u64> selector;
    BitField<47, 1, u64> cc;
    BitField<45, 1, u64> neg;
    BitField<49, 1, u64> abs;
};

bool Is64(u64 insn) {
    return Encoding{insn}.int_format == IntFormat::U64;
}

int BitSize(FloatFormat format) {
    switch (format) {
    case FloatFormat::F16:
        return 16;
    case FloatFormat::F32:
        return 32;
    case FloatFormat::F64:
        return 64;
    }
    throw NotImplementedException("Invalid float format {}", format);
}

IR::U32 SmallAbs(TranslatorVisitor& v, const IR::U32& value, int bitsize) {
    const IR::U32 least_value{v.ir.Imm32(-(1 << (bitsize - 1)))};
    const IR::U32 mask{v.ir.ShiftRightArithmetic(value, v.ir.Imm32(bitsize - 1))};
    const IR::U32 absolute{v.ir.BitwiseXor(v.ir.IAdd(value, mask), mask)};
    const IR::U1 is_least{v.ir.IEqual(value, least_value)};
    return IR::U32{v.ir.Select(is_least, value, absolute)};
}

void I2F(TranslatorVisitor& v, u64 insn, IR::U32U64 src) {
    const Encoding i2f{insn};
    if (i2f.cc != 0) {
        throw NotImplementedException("I2F CC");
    }
    const bool is_signed{i2f.is_signed != 0};
    int src_bitsize{};
    switch (i2f.int_format) {
    case IntFormat::U8:
        src = v.ir.BitFieldExtract(src, v.ir.Imm32(static_cast<u32>(i2f.selector) * 8),
                                   v.ir.Imm32(8), is_signed);
        if (i2f.abs != 0) {
            src = SmallAbs(v, src, 8);
        }
        src_bitsize = 8;
        break;
    case IntFormat::U16:
        if (i2f.selector == 1 || i2f.selector == 3) {
            throw NotImplementedException("Invalid U16 selector {}", i2f.selector.Value());
        }
        src = v.ir.BitFieldExtract(src, v.ir.Imm32(static_cast<u32>(i2f.selector) * 8),
                                   v.ir.Imm32(16), is_signed);
        if (i2f.abs != 0) {
            src = SmallAbs(v, src, 16);
        }
        src_bitsize = 16;
        break;
    case IntFormat::U32:
    case IntFormat::U64:
        if (i2f.selector != 0) {
            throw NotImplementedException("Unexpected selector {}", i2f.selector.Value());
        }
        if (i2f.abs != 0 && is_signed) {
            src = v.ir.IAbs(src);
        }
        src_bitsize = i2f.int_format == IntFormat::U64 ? 64 : 32;
        break;
    }
    const int conversion_src_bitsize{i2f.int_format == IntFormat::U64 ? 64 : 32};
    const int dst_bitsize{BitSize(i2f.float_format)};
    const IR::FpControl fp_control{
        .no_contraction = false,
        .rounding = CastFpRounding(i2f.fp_rounding),
        .fmz_mode = IR::FmzMode::DontCare,
    };
    auto value{v.ir.ConvertIToF(static_cast<size_t>(dst_bitsize),
                                static_cast<size_t>(conversion_src_bitsize), is_signed, src,
                                fp_control)};
    if (i2f.neg != 0) {
        if (i2f.abs != 0 || !is_signed) {
            // We know the value is positive
            value = v.ir.FPNeg(value);
        } else {
            // Only negate if the input isn't the lowest value
            IR::U1 is_least;
            if (src_bitsize == 64) {
                is_least = v.ir.IEqual(src, v.ir.Imm64(std::numeric_limits<s64>::min()));
            } else if (src_bitsize == 32) {
                is_least = v.ir.IEqual(src, v.ir.Imm32(std::numeric_limits<s32>::min()));
            } else {
                const IR::U32 least_value{v.ir.Imm32(-(1 << (src_bitsize - 1)))};
                is_least = v.ir.IEqual(src, least_value);
            }
            value = IR::F16F32F64{v.ir.Select(is_least, value, v.ir.FPNeg(value))};
        }
    }
    switch (i2f.float_format) {
    case FloatFormat::F16: {
        const IR::F16 zero{v.ir.FPConvert(16, v.ir.Imm32(0.0f))};
        v.X(i2f.dest_reg, v.ir.PackFloat2x16(v.ir.CompositeConstruct(value, zero)));
        break;
    }
    case FloatFormat::F32:
        v.F(i2f.dest_reg, value);
        break;
    case FloatFormat::F64: {
        if (!IR::IsAligned(i2f.dest_reg, 2)) {
            throw NotImplementedException("Unaligned destination {}", i2f.dest_reg.Value());
        }
        const IR::Value vector{v.ir.UnpackDouble2x32(value)};
        for (int i = 0; i < 2; ++i) {
            v.X(i2f.dest_reg + i, IR::U32{v.ir.CompositeExtract(vector, static_cast<size_t>(i))});
        }
        break;
    }
    default:
        throw NotImplementedException("Invalid float format {}", i2f.float_format.Value());
    }
}
} // Anonymous namespace

void TranslatorVisitor::I2F_reg(u64 insn) {
    if (Is64(insn)) {
        union {
            u64 raw;
            BitField<20, 8, IR::Reg> reg;
        } const value{insn};
        const IR::Value regs{ir.CompositeConstruct(ir.GetReg(value.reg), ir.GetReg(value.reg + 1))};
        I2F(*this, insn, ir.PackUint2x32(regs));
    } else {
        I2F(*this, insn, GetReg20(insn));
    }
}

void TranslatorVisitor::I2F_cbuf(u64 insn) {
    if (Is64(insn)) {
        I2F(*this, insn, GetPackedCbuf(insn));
    } else {
        I2F(*this, insn, GetCbuf(insn));
    }
}

void TranslatorVisitor::I2F_imm(u64 insn) {
    if (Is64(insn)) {
        I2F(*this, insn, GetPackedImm20(insn));
    } else {
        I2F(*this, insn, GetImm20(insn));
    }
}

} // namespace Shader::Maxwell
