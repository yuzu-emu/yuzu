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
        .no_contraction = false,
        .rounding = IR::FpRounding::RN,
        .fmz_mode = IR::FmzMode::DontCare,
    };
    static constexpr IR::FpControl f32_control{
        .no_contraction = false,
        .rounding = IR::FpRounding::RN,
        .fmz_mode = IR::FmzMode::FTZ,
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

IR::Value ApplyAtomOp(TranslatorVisitor& v, IR::Reg operand_reg, const IR::U64& offset,
                      AtomSize size, AtomOp op) {
    switch (size) {
    case AtomSize::U32:
    case AtomSize::S32:
        return ApplyIntegerAtomOp(v.ir, offset, v.X(operand_reg), op, size == AtomSize::S32);
    case AtomSize::U64:
    case AtomSize::S64:
        return ApplyIntegerAtomOp(v.ir, offset, v.L(operand_reg), op, size == AtomSize::S64);
    case AtomSize::F32:
        return ApplyFpAtomOp(v.ir, offset, v.F(operand_reg), op, size);
    case AtomSize::F16x2: {
        return ApplyFpAtomOp(v.ir, offset, v.ir.UnpackFloat2x16(v.X(operand_reg)), op, size);
    }
    default:
        throw NotImplementedException("Atom Size {}", size);
    }
}

void GlobalAtomic(TranslatorVisitor& v, IR::Reg dest_reg, IR::Reg operand_reg,
                  const IR::U64& offset, AtomSize size, AtomOp op, bool write_dest) {
    IR::Value result;
    if (AtomOpNotApplicable(size, op)) {
        result = LoadGlobal(v.ir, offset, size);
    } else {
        result = ApplyAtomOp(v, operand_reg, offset, size, op);
    }
    if (write_dest) {
        StoreResult(v, dest_reg, result, size);
    }
}
} // Anonymous namespace

void TranslatorVisitor::ATOM(u64 insn) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<20, 8, IR::Reg> operand_reg;
        BitField<49, 3, AtomSize> size;
        BitField<52, 4, AtomOp> op;
    } const atom{insn};
    const IR::U64 offset{AtomOffset(*this, insn)};
    GlobalAtomic(*this, atom.dest_reg, atom.operand_reg, offset, atom.size, atom.op, true);
}

void TranslatorVisitor::RED(u64 insn) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> operand_reg;
        BitField<20, 3, AtomSize> size;
        BitField<23, 3, AtomOp> op;
    } const red{insn};
    const IR::U64 offset{AtomOffset(*this, insn)};
    GlobalAtomic(*this, IR::Reg::RZ, red.operand_reg, offset, red.size, red.op, true);
}

} // namespace Shader::Maxwell
