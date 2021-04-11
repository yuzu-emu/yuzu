// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class AtomOp : u64 {
    ADD,
    MIN,
    MAX,
    INC,
    DEC,
    AND,
    OR,
    XOR,
    EXCH,
    SAFEADD,
};

enum class AtomSize : u64 {
    U32,
    S32,
    U64,
    F32,
    F16x2,
    S64,
};

IR::U32U64 ApplyIntegerAtomOp(IR::IREmitter& ir, const IR::U32U64& offset, const IR::U32U64& op_b,
                              AtomOp op, bool is_signed) {
    switch (op) {
    case AtomOp::ADD:
        return ir.GlobalAtomicIAdd(offset, op_b);
    case AtomOp::MIN:
        return ir.GlobalAtomicIMin(offset, op_b, is_signed);
    case AtomOp::MAX:
        return ir.GlobalAtomicIMax(offset, op_b, is_signed);
    case AtomOp::INC:
        return ir.GlobalAtomicInc(offset, op_b);
    case AtomOp::DEC:
        return ir.GlobalAtomicDec(offset, op_b);
    case AtomOp::AND:
        return ir.GlobalAtomicAnd(offset, op_b);
    case AtomOp::OR:
        return ir.GlobalAtomicOr(offset, op_b);
    case AtomOp::XOR:
        return ir.GlobalAtomicXor(offset, op_b);
    case AtomOp::EXCH:
        return ir.GlobalAtomicExchange(offset, op_b);
    default:
        throw NotImplementedException("Integer Atom Operation {}", op);
    }
}

IR::Value ApplyFpAtomOp(IR::IREmitter& ir, const IR::U64& offset, const IR::Value& op_b, AtomOp op,
                        AtomSize size) {
    static constexpr IR::FpControl f16_control{
        .no_contraction{false},
        .rounding{IR::FpRounding::RN},
        .fmz_mode{IR::FmzMode::DontCare},
    };
    static constexpr IR::FpControl f32_control{
        .no_contraction{false},
        .rounding{IR::FpRounding::RN},
        .fmz_mode{IR::FmzMode::FTZ},
    };
    switch (op) {
    case AtomOp::ADD:
        return size == AtomSize::F32 ? ir.GlobalAtomicF32Add(offset, op_b, f32_control)
                                     : ir.GlobalAtomicF16x2Add(offset, op_b, f16_control);
    case AtomOp::MIN:
        return ir.GlobalAtomicF16x2Min(offset, op_b, f16_control);
    case AtomOp::MAX:
        return ir.GlobalAtomicF16x2Max(offset, op_b, f16_control);
    default:
        throw NotImplementedException("FP Atom Operation {}", op);
    }
}

IR::U64 AtomOffset(TranslatorVisitor& v, u64 insn) {
    union {
        u64 raw;
        BitField<8, 8, IR::Reg> addr_reg;
        BitField<28, 20, s64> addr_offset;
        BitField<28, 20, u64> rz_addr_offset;
        BitField<48, 1, u64> e;
    } const mem{insn};

    const IR::U64 address{[&]() -> IR::U64 {
        if (mem.e == 0) {
            return v.ir.UConvert(64, v.X(mem.addr_reg));
        }
        return v.L(mem.addr_reg);
    }()};
    const u64 addr_offset{[&]() -> u64 {
        if (mem.addr_reg == IR::Reg::RZ) {
            // When RZ is used, the address is an absolute address
            return static_cast<u64>(mem.rz_addr_offset.Value());
        } else {
            return static_cast<u64>(mem.addr_offset.Value());
        }
    }()};
    return v.ir.IAdd(address, v.ir.Imm64(addr_offset));
}

bool AtomOpNotApplicable(AtomSize size, AtomOp op) {
    // TODO: SAFEADD
    switch (size) {
    case AtomSize::S32:
    case AtomSize::U64:
        return (op == AtomOp::INC || op == AtomOp::DEC);
    case AtomSize::S64:
        return !(op == AtomOp::MIN || op == AtomOp::MAX);
    case AtomSize::F32:
        return op != AtomOp::ADD;
    case AtomSize::F16x2:
        return !(op == AtomOp::ADD || op == AtomOp::MIN || op == AtomOp::MAX);
    default:
        return false;
    }
}

IR::U32U64 LoadGlobal(IR::IREmitter& ir, const IR::U64& offset, AtomSize size) {
    switch (size) {
    case AtomSize::U32:
    case AtomSize::S32:
    case AtomSize::F32:
    case AtomSize::F16x2:
        return ir.LoadGlobal32(offset);
    case AtomSize::U64:
    case AtomSize::S64:
        return ir.PackUint2x32(ir.LoadGlobal64(offset));
    default:
        throw NotImplementedException("Atom Size {}", size);
    }
}

void StoreResult(TranslatorVisitor& v, IR::Reg dest_reg, const IR::Value& result, AtomSize size) {
    switch (size) {
    case AtomSize::U32:
    case AtomSize::S32:
    case AtomSize::F16x2:
        return v.X(dest_reg, IR::U32{result});
    case AtomSize::U64:
    case AtomSize::S64:
        return v.L(dest_reg, IR::U64{result});
    case AtomSize::F32:
        return v.F(dest_reg, IR::F32{result});
    default:
        break;
    }
}
} // Anonymous namespace

void TranslatorVisitor::ATOM(u64 insn) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> addr_reg;
        BitField<20, 8, IR::Reg> src_reg_b;
        BitField<49, 3, AtomSize> size;
        BitField<52, 4, AtomOp> op;
    } const atom{insn};

    const bool size_64{atom.size == AtomSize::U64 || atom.size == AtomSize::S64};
    const bool is_signed{atom.size == AtomSize::S32 || atom.size == AtomSize::S64};
    const bool is_integer{atom.size != AtomSize::F32 && atom.size != AtomSize::F16x2};
    const IR::U64 offset{AtomOffset(*this, insn)};
    IR::Value result;

    if (AtomOpNotApplicable(atom.size, atom.op)) {
        result = LoadGlobal(ir, offset, atom.size);
    } else if (!is_integer) {
        if (atom.size == AtomSize::F32) {
            result = ApplyFpAtomOp(ir, offset, F(atom.src_reg_b), atom.op, atom.size);
        } else {
            const IR::Value src_b{ir.UnpackFloat2x16(X(atom.src_reg_b))};
            result = ApplyFpAtomOp(ir, offset, src_b, atom.op, atom.size);
        }
    } else if (size_64) {
        result = ApplyIntegerAtomOp(ir, offset, L(atom.src_reg_b), atom.op, is_signed);
    } else {
        result = ApplyIntegerAtomOp(ir, offset, X(atom.src_reg_b), atom.op, is_signed);
    }
    StoreResult(*this, atom.dest_reg, result, atom.size);
}

void TranslatorVisitor::RED(u64 insn) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> src_reg_b;
        BitField<8, 8, IR::Reg> addr_reg;
        BitField<20, 3, AtomSize> size;
        BitField<23, 3, AtomOp> op;
    } const red{insn};

    if (AtomOpNotApplicable(red.size, red.op)) {
        return;
    }
    const bool size_64{red.size == AtomSize::U64 || red.size == AtomSize::S64};
    const bool is_signed{red.size == AtomSize::S32 || red.size == AtomSize::S64};
    const bool is_integer{red.size != AtomSize::F32 && red.size != AtomSize::F16x2};
    const IR::U64 offset{AtomOffset(*this, insn)};
    if (!is_integer) {
        if (red.size == AtomSize::F32) {
            ApplyFpAtomOp(ir, offset, F(red.src_reg_b), red.op, red.size);
        } else {
            const IR::Value src_b{ir.UnpackFloat2x16(X(red.src_reg_b))};
            ApplyFpAtomOp(ir, offset, src_b, red.op, red.size);
        }
    } else if (size_64) {
        ApplyIntegerAtomOp(ir, offset, L(red.src_reg_b), red.op, is_signed);
    } else {
        ApplyIntegerAtomOp(ir, offset, X(red.src_reg_b), red.op, is_signed);
    }
}

} // namespace Shader::Maxwell
