// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <limits>

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
    BitField<39, 2, Rounding> rounding;
    BitField<41, 1, u64> half;
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

std::pair<f64, f64> ClampBounds(DestFormat format, bool is_signed) {
    if (is_signed) {
        switch (format) {
        case DestFormat::I16:
            return {static_cast<f64>(std::numeric_limits<s16>::max()),
                    static_cast<f64>(std::numeric_limits<s16>::min())};
        case DestFormat::I32:
            return {static_cast<f64>(std::numeric_limits<s32>::max()),
                    static_cast<f64>(std::numeric_limits<s32>::min())};
        case DestFormat::I64:
            return {static_cast<f64>(std::numeric_limits<s64>::max()),
                    static_cast<f64>(std::numeric_limits<s64>::min())};
        default:
            break;
        }
    } else {
        switch (format) {
        case DestFormat::I16:
            return {static_cast<f64>(std::numeric_limits<u16>::max()),
                    static_cast<f64>(std::numeric_limits<u16>::min())};
        case DestFormat::I32:
            return {static_cast<f64>(std::numeric_limits<u32>::max()),
                    static_cast<f64>(std::numeric_limits<u32>::min())};
        case DestFormat::I64:
            return {static_cast<f64>(std::numeric_limits<u64>::max()),
                    static_cast<f64>(std::numeric_limits<u64>::min())};
        default:
            break;
        }
    }
    throw NotImplementedException("Invalid destination format {}", format);
}

IR::F64 UnpackCbuf(TranslatorVisitor& v, u64 insn) {
    union {
        u64 raw;
        BitField<20, 14, s64> offset;
        BitField<34, 5, u64> binding;
    } const cbuf{insn};
    if (cbuf.binding >= 18) {
        throw NotImplementedException("Out of bounds constant buffer binding {}", cbuf.binding);
    }
    if (cbuf.offset >= 0x4'000 || cbuf.offset < 0) {
        throw NotImplementedException("Out of bounds constant buffer offset {}", cbuf.offset * 4);
    }
    if (cbuf.offset % 2 != 0) {
        throw NotImplementedException("Unaligned F64 constant buffer offset {}", cbuf.offset * 4);
    }
    const IR::U32 binding{v.ir.Imm32(static_cast<u32>(cbuf.binding))};
    const IR::U32 byte_offset{v.ir.Imm32(static_cast<u32>(cbuf.offset) * 4 + 4)};
    const IR::U32 cbuf_data{v.ir.GetCbuf(binding, byte_offset)};
    const IR::Value vector{v.ir.CompositeConstruct(v.ir.Imm32(0U), cbuf_data)};
    return v.ir.PackDouble2x32(vector);
}

void TranslateF2I(TranslatorVisitor& v, u64 insn, const IR::F16F32F64& src_a) {
    // F2I is used to convert from a floating point value to an integer
    const F2I f2i{insn};

    const bool denorm_cares{f2i.src_format != SrcFormat::F16 && f2i.src_format != SrcFormat::F64 &&
                            f2i.dest_format != DestFormat::I64};
    IR::FmzMode fmz_mode{IR::FmzMode::DontCare};
    if (denorm_cares) {
        fmz_mode = f2i.ftz != 0 ? IR::FmzMode::FTZ : IR::FmzMode::None;
    }
    const IR::FpControl fp_control{
        .no_contraction = true,
        .rounding = IR::FpRounding::DontCare,
        .fmz_mode = fmz_mode,
    };
    const IR::F16F32F64 op_a{v.ir.FPAbsNeg(src_a, f2i.abs != 0, f2i.neg != 0)};
    const IR::F16F32F64 rounded_value{[&] {
        switch (f2i.rounding) {
        case Rounding::Round:
            return v.ir.FPRoundEven(op_a, fp_control);
        case Rounding::Floor:
            return v.ir.FPFloor(op_a, fp_control);
        case Rounding::Ceil:
            return v.ir.FPCeil(op_a, fp_control);
        case Rounding::Trunc:
            return v.ir.FPTrunc(op_a, fp_control);
        default:
            throw NotImplementedException("Invalid F2I rounding {}", f2i.rounding.Value());
        }
    }()};
    const bool is_signed{f2i.is_signed != 0};
    const auto [max_bound, min_bound] = ClampBounds(f2i.dest_format, is_signed);

    IR::F16F32F64 intermediate;
    switch (f2i.src_format) {
    case SrcFormat::F16: {
        const IR::F16 max_val{v.ir.FPConvert(16, v.ir.Imm32(static_cast<f32>(max_bound)))};
        const IR::F16 min_val{v.ir.FPConvert(16, v.ir.Imm32(static_cast<f32>(min_bound)))};
        intermediate = v.ir.FPClamp(rounded_value, min_val, max_val);
        break;
    }
    case SrcFormat::F32: {
        const IR::F32 max_val{v.ir.Imm32(static_cast<f32>(max_bound))};
        const IR::F32 min_val{v.ir.Imm32(static_cast<f32>(min_bound))};
        intermediate = v.ir.FPClamp(rounded_value, min_val, max_val);
        break;
    }
    case SrcFormat::F64: {
        const IR::F64 max_val{v.ir.Imm64(max_bound)};
        const IR::F64 min_val{v.ir.Imm64(min_bound)};
        intermediate = v.ir.FPClamp(rounded_value, min_val, max_val);
        break;
    }
    default:
        throw NotImplementedException("Invalid destination format {}", f2i.dest_format.Value());
    }

    const size_t bitsize{std::max<size_t>(32, BitSize(f2i.dest_format))};
    IR::U16U32U64 result{v.ir.ConvertFToI(bitsize, is_signed, intermediate)};

    bool handled_special_case = false;
    const bool special_nan_cases =
        (f2i.src_format == SrcFormat::F64) != (f2i.dest_format == DestFormat::I64);
    if (special_nan_cases) {
        if (f2i.dest_format == DestFormat::I32) {
            handled_special_case = true;
            result = IR::U32{v.ir.Select(v.ir.FPIsNan(op_a), v.ir.Imm32(0x8000'0000U), result)};
        } else if (f2i.dest_format == DestFormat::I64) {
            handled_special_case = true;
            result = IR::U64{
                v.ir.Select(v.ir.FPIsNan(op_a), v.ir.Imm64(0x8000'0000'0000'0000UL), result)};
        }
    }
    if (!handled_special_case && is_signed) {
        if (bitsize != 64) {
            result = IR::U32{v.ir.Select(v.ir.FPIsNan(op_a), v.ir.Imm32(0U), result)};
        } else {
            result = IR::U64{v.ir.Select(v.ir.FPIsNan(op_a), v.ir.Imm64(u64{0}), result)};
        }
    }

    if (bitsize == 64) {
        v.L(f2i.dest_reg, result);
    } else {
        v.X(f2i.dest_reg, result);
    }

    if (f2i.cc != 0) {
        throw NotImplementedException("F2I CC");
    }
}
} // Anonymous namespace

void TranslatorVisitor::F2I_reg(u64 insn) {
    union {
        u64 raw;
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

void TranslatorVisitor::F2I_cbuf(u64 insn) {
    const F2I f2i{insn};
    const IR::F16F32F64 op_a{[&]() -> IR::F16F32F64 {
        switch (f2i.src_format) {
        case SrcFormat::F16:
            return IR::F16{ir.CompositeExtract(ir.UnpackFloat2x16(GetCbuf(insn)), f2i.half)};
        case SrcFormat::F32:
            return GetFloatCbuf(insn);
        case SrcFormat::F64: {
            return UnpackCbuf(*this, insn);
        }
        default:
            throw NotImplementedException("Invalid F2I source format {}", f2i.src_format.Value());
        }
    }()};
    TranslateF2I(*this, insn, op_a);
}

void TranslatorVisitor::F2I_imm(u64) {
    throw NotImplementedException("{}", Opcode::F2I_imm);
}

} // namespace Shader::Maxwell
