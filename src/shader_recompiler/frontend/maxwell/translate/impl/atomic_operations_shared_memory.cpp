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
};

enum class AtomsSize : u64 {
    U32,
    S32,
    U64,
};

IR::U32U64 ApplyAtomsOp(IR::IREmitter& ir, const IR::U32& offset, const IR::U32U64& op_b, AtomOp op,
                        bool is_signed) {
    switch (op) {
    case AtomOp::ADD:
        return ir.SharedAtomicIAdd(offset, op_b);
    case AtomOp::MIN:
        return ir.SharedAtomicIMin(offset, op_b, is_signed);
    case AtomOp::MAX:
        return ir.SharedAtomicIMax(offset, op_b, is_signed);
    case AtomOp::INC:
        return ir.SharedAtomicInc(offset, op_b);
    case AtomOp::DEC:
        return ir.SharedAtomicDec(offset, op_b);
    case AtomOp::AND:
        return ir.SharedAtomicAnd(offset, op_b);
    case AtomOp::OR:
        return ir.SharedAtomicOr(offset, op_b);
    case AtomOp::XOR:
        return ir.SharedAtomicXor(offset, op_b);
    case AtomOp::EXCH:
        return ir.SharedAtomicExchange(offset, op_b);
    default:
        throw NotImplementedException("Integer Atoms Operation {}", op);
    }
}

IR::U32 AtomsOffset(TranslatorVisitor& v, u64 insn) {
    union {
        u64 raw;
        BitField<8, 8, IR::Reg> offset_reg;
        BitField<30, 22, u64> absolute_offset;
        BitField<30, 22, s64> relative_offset;
    } const encoding{insn};

    if (encoding.offset_reg == IR::Reg::RZ) {
        return v.ir.Imm32(static_cast<u32>(encoding.absolute_offset << 2));
    } else {
        const s32 relative{static_cast<s32>(encoding.relative_offset << 2)};
        return v.ir.IAdd(v.X(encoding.offset_reg), v.ir.Imm32(relative));
    }
}

void StoreResult(TranslatorVisitor& v, IR::Reg dest_reg, const IR::Value& result, AtomsSize size) {
    switch (size) {
    case AtomsSize::U32:
    case AtomsSize::S32:
        return v.X(dest_reg, IR::U32{result});
    case AtomsSize::U64:
        return v.L(dest_reg, IR::U64{result});
    default:
        break;
    }
}
} // Anonymous namespace

void TranslatorVisitor::ATOMS(u64 insn) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> addr_reg;
        BitField<20, 8, IR::Reg> src_reg_b;
        BitField<28, 2, AtomsSize> size;
        BitField<52, 4, AtomOp> op;
    } const atoms{insn};

    const bool size_64{atoms.size == AtomsSize::U64};
    if (size_64 && atoms.op != AtomOp::EXCH) {
        throw NotImplementedException("64-bit Atoms Operation {}", atoms.op.Value());
    }
    const bool is_signed{atoms.size == AtomsSize::S32};
    const IR::U32 offset{AtomsOffset(*this, insn)};

    IR::Value result;
    if (size_64) {
        result = ApplyAtomsOp(ir, offset, L(atoms.src_reg_b), atoms.op, is_signed);
    } else {
        result = ApplyAtomsOp(ir, offset, X(atoms.src_reg_b), atoms.op, is_signed);
    }
    StoreResult(*this, atoms.dest_reg, result, atoms.size);
}

} // namespace Shader::Maxwell
