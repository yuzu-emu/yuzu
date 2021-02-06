// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/maxwell/opcodes.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class DestFormat : u64 {
    Invalid,
    I16,
    I32,
    I64,
};
enum class SrcFormat : u64 {
    Invalid,
    F16,
    F32,
    F64,
};
enum class Rounding : u64 {
    Round,
    Floor,
    Ceil,
    Trunc,
};

union F2I {
    u64 raw;
    BitField<0, 8, IR::Reg> dest_reg;
    BitField<8, 2, DestFormat> dest_format;
    BitField<10, 2, SrcFormat> src_format;
    BitField<12, 1, u64> is_signed;
    BitField<39, 1, Rounding> rounding;
    BitField<49, 1, u64> half;
    BitField<44, 1, u64> ftz;
    BitField<45, 1, u64> abs;
    BitField<47, 1, u64> cc;
    BitField<49, 1, u64> neg;
};

size_t BitSize(DestFormat dest_format) {
    switch (dest_format) {
    case DestFormat::I16:
        return 16;
    case DestFormat::I32:
        return 32;
    case DestFormat::I64:
        return 64;
    default:
        throw NotImplementedException("Invalid destination format {}", dest_format);
    }
}

void TranslateF2I(TranslatorVisitor& v, u64 insn, const IR::F16F32F64& src_a) {
    // F2I is used to convert from a floating point value to an integer
    const F2I f2i{insn};

    const IR::F16F32F64 op_a{v.ir.FPAbsNeg(src_a, f2i.abs != 0, f2i.neg != 0)};
    const IR::F16F32F64 rounded_value{[&] {
        switch (f2i.rounding) {
        case Rounding::Round:
            return v.ir.FPRoundEven(op_a);
        case Rounding::Floor:
            return v.ir.FPFloor(op_a);
        case Rounding::Ceil:
            return v.ir.FPCeil(op_a);
        case Rounding::Trunc:
            return v.ir.FPTrunc(op_a);
        default:
            throw NotImplementedException("Invalid F2I rounding {}", f2i.rounding.Value());
        }
    }()};

    // TODO: Handle out of bounds conversions.
    // For example converting F32 65537.0 to U16, the expected value is 0xffff,

    const bool is_signed{f2i.is_signed != 0};
    const size_t bitsize{BitSize(f2i.dest_format)};
    const IR::U16U32U64 result{v.ir.ConvertFToI(bitsize, is_signed, rounded_value)};

    v.X(f2i.dest_reg, result);

    if (f2i.cc != 0) {
        v.SetZFlag(v.ir.GetZeroFromOp(result));
        if (is_signed) {
            v.SetSFlag(v.ir.GetSignFromOp(result));
        } else {
            v.ResetSFlag();
        }
        v.ResetCFlag();

        // TODO: Investigate if out of bound conversions sets the overflow flag
        v.ResetOFlag();
    }
}
} // Anonymous namespace

void TranslatorVisitor::F2I_reg(u64 insn) {
    union {
        F2I base;
        BitField<20, 8, IR::Reg> src_reg;
    } const f2i{insn};

    const IR::F16F32F64 op_a{[&]() -> IR::F16F32F64 {
        switch (f2i.base.src_format) {
        case SrcFormat::F16:
            return IR::F16{ir.CompositeExtract(ir.UnpackFloat2x16(X(f2i.src_reg)), f2i.base.half)};
        case SrcFormat::F32:
            return F(f2i.src_reg);
        case SrcFormat::F64:
            return ir.PackDouble2x32(ir.CompositeConstruct(X(f2i.src_reg), X(f2i.src_reg + 1)));
        default:
            throw NotImplementedException("Invalid F2I source format {}",
                                          f2i.base.src_format.Value());
        }
    }()};

    TranslateF2I(*this, insn, op_a);
}

void TranslatorVisitor::F2I_cbuf(u64) {
    throw NotImplementedException("{}", Opcode::F2I_cbuf);
}

void TranslatorVisitor::F2I_imm(u64) {
    throw NotImplementedException("{}", Opcode::F2I_imm);
}

} // namespace Shader::Maxwell
