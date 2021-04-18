// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/frontend/maxwell/translate/impl/common_encoding.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/half_floating_point_helper.h"

namespace Shader::Maxwell {
namespace {
enum class FloatFormat : u64 {
    F16 = 1,
    F32 = 2,
    F64 = 3,
};

enum class RoundingOp : u64 {
    None = 0,
    Pass = 3,
    Round = 8,
    Floor = 9,
    Ceil = 10,
    Trunc = 11,
};

[[nodiscard]] u32 WidthSize(FloatFormat width) {
    switch (width) {
    case FloatFormat::F16:
        return 16;
    case FloatFormat::F32:
        return 32;
    case FloatFormat::F64:
        return 64;
    default:
        throw NotImplementedException("Invalid width {}", width);
    }
}

void F2F(TranslatorVisitor& v, u64 insn, const IR::F16F32F64& src_a, bool abs) {
    union {
        u64 insn;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<44, 1, u64> ftz;
        BitField<45, 1, u64> neg;
        BitField<47, 1, u64> cc;
        BitField<50, 1, u64> sat;
        BitField<39, 4, u64> rounding_op;
        BitField<39, 2, FpRounding> rounding;
        BitField<10, 2, FloatFormat> src_size;
        BitField<8, 2, FloatFormat> dst_size;

        [[nodiscard]] RoundingOp RoundingOperation() const {
            constexpr u64 rounding_mask = 0x0B;
            return static_cast<RoundingOp>(rounding_op.Value() & rounding_mask);
        }
    } const f2f{insn};

    if (f2f.cc != 0) {
        throw NotImplementedException("F2F CC");
    }

    IR::F16F32F64 input{v.ir.FPAbsNeg(src_a, abs, f2f.neg != 0)};

    const bool any_fp64{f2f.src_size == FloatFormat::F64 || f2f.dst_size == FloatFormat::F64};
    IR::FpControl fp_control{
        .no_contraction = false,
        .rounding = IR::FpRounding::DontCare,
        .fmz_mode = (f2f.ftz != 0 && !any_fp64 ? IR::FmzMode::FTZ : IR::FmzMode::None),
    };
    if (f2f.src_size != f2f.dst_size) {
        fp_control.rounding = CastFpRounding(f2f.rounding);
        input = v.ir.FPConvert(WidthSize(f2f.dst_size), input, fp_control);
    } else {
        switch (f2f.RoundingOperation()) {
        case RoundingOp::None:
        case RoundingOp::Pass:
            // Make sure NANs are handled properly
            switch (f2f.src_size) {
            case FloatFormat::F16:
                input = v.ir.FPAdd(input, v.ir.FPConvert(16, v.ir.Imm32(0.0f)), fp_control);
                break;
            case FloatFormat::F32:
                input = v.ir.FPAdd(input, v.ir.Imm32(0.0f), fp_control);
                break;
            case FloatFormat::F64:
                input = v.ir.FPAdd(input, v.ir.Imm64(0.0), fp_control);
                break;
            }
            break;
        case RoundingOp::Round:
            input = v.ir.FPRoundEven(input, fp_control);
            break;
        case RoundingOp::Floor:
            input = v.ir.FPFloor(input, fp_control);
            break;
        case RoundingOp::Ceil:
            input = v.ir.FPCeil(input, fp_control);
            break;
        case RoundingOp::Trunc:
            input = v.ir.FPTrunc(input, fp_control);
            break;
        default:
            throw NotImplementedException("Unimplemented rounding mode {}", f2f.rounding.Value());
        }
    }
    if (f2f.sat != 0 && !any_fp64) {
        input = v.ir.FPSaturate(input);
    }

    switch (f2f.dst_size) {
    case FloatFormat::F16: {
        const IR::F16 imm{v.ir.FPConvert(16, v.ir.Imm32(0.0f))};
        v.X(f2f.dest_reg, v.ir.PackFloat2x16(v.ir.CompositeConstruct(input, imm)));
        break;
    }
    case FloatFormat::F32:
        v.F(f2f.dest_reg, input);
        break;
    case FloatFormat::F64:
        v.D(f2f.dest_reg, input);
        break;
    default:
        throw NotImplementedException("Invalid dest format {}", f2f.dst_size.Value());
    }
}
} // Anonymous namespace

void TranslatorVisitor::F2F_reg(u64 insn) {
    union {
        u64 insn;
        BitField<49, 1, u64> abs;
        BitField<10, 2, FloatFormat> src_size;
        BitField<41, 1, u64> selector;
    } const f2f{insn};

    IR::F16F32F64 src_a;
    switch (f2f.src_size) {
    case FloatFormat::F16: {
        auto [lhs_a, rhs_a]{Extract(ir, GetReg20(insn), Swizzle::H1_H0)};
        src_a = f2f.selector != 0 ? rhs_a : lhs_a;
        break;
    }
    case FloatFormat::F32:
        src_a = GetFloatReg20(insn);
        break;
    case FloatFormat::F64:
        src_a = GetDoubleReg20(insn);
        break;
    default:
        throw NotImplementedException("Invalid dest format {}", f2f.src_size.Value());
    }
    F2F(*this, insn, src_a, f2f.abs != 0);
}

void TranslatorVisitor::F2F_cbuf(u64 insn) {
    union {
        u64 insn;
        BitField<49, 1, u64> abs;
        BitField<10, 2, FloatFormat> src_size;
        BitField<41, 1, u64> selector;
    } const f2f{insn};

    IR::F16F32F64 src_a;
    switch (f2f.src_size) {
    case FloatFormat::F16: {
        auto [lhs_a, rhs_a]{Extract(ir, GetCbuf(insn), Swizzle::H1_H0)};
        src_a = f2f.selector != 0 ? rhs_a : lhs_a;
        break;
    }
    case FloatFormat::F32:
        src_a = GetFloatCbuf(insn);
        break;
    case FloatFormat::F64:
        src_a = GetDoubleCbuf(insn);
        break;
    default:
        throw NotImplementedException("Invalid dest format {}", f2f.src_size.Value());
    }
    F2F(*this, insn, src_a, f2f.abs != 0);
}

void TranslatorVisitor::F2F_imm([[maybe_unused]] u64 insn) {
    union {
        u64 insn;
        BitField<49, 1, u64> abs;
        BitField<10, 2, FloatFormat> src_size;
        BitField<41, 1, u64> selector;
        BitField<20, 19, u64> imm;
        BitField<56, 1, u64> imm_neg;
    } const f2f{insn};

    IR::F16F32F64 src_a;
    switch (f2f.src_size) {
    case FloatFormat::F16: {
        const u32 imm{static_cast<u32>(f2f.imm & 0x0000ffff)};
        const IR::Value vector{ir.UnpackFloat2x16(ir.Imm32(imm | (imm << 16)))};
        src_a = IR::F16{ir.CompositeExtract(vector, f2f.selector != 0 ? 0 : 1)};
        if (f2f.imm_neg != 0) {
            throw NotImplementedException("Neg bit on F16");
        }
        break;
    }
    case FloatFormat::F32:
        src_a = GetFloatImm20(insn);
        break;
    case FloatFormat::F64:
        src_a = GetDoubleImm20(insn);
        break;
    default:
        throw NotImplementedException("Invalid dest format {}", f2f.src_size.Value());
    }
    F2F(*this, insn, src_a, f2f.abs != 0);
}

} // namespace Shader::Maxwell
