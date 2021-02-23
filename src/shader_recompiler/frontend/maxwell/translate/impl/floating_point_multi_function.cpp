// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/maxwell/opcodes.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class Operation : u64 {
    Cos = 0,
    Sin = 1,
    Ex2 = 2,    // Base 2 exponent
    Lg2 = 3,    // Base 2 logarithm
    Rcp = 4,    // Reciprocal
    Rsq = 5,    // Reciprocal square root
    Rcp64H = 6, // 64-bit reciprocal
    Rsq64H = 7, // 64-bit reciprocal square root
    Sqrt = 8,
};
} // Anonymous namespace

void TranslatorVisitor::MUFU(u64 insn) {
    // MUFU is used to implement a bunch of special functions. See Operation.
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_reg;
        BitField<20, 4, Operation> operation;
        BitField<46, 1, u64> abs;
        BitField<48, 1, u64> neg;
        BitField<50, 1, u64> sat;
    } const mufu{insn};

    const IR::F32 op_a{ir.FPAbsNeg(F(mufu.src_reg), mufu.abs != 0, mufu.neg != 0)};
    IR::F32 value{[&]() -> IR::F32 {
        switch (mufu.operation) {
        case Operation::Cos:
            return ir.FPCos(op_a);
        case Operation::Sin:
            return ir.FPSin(op_a);
        case Operation::Ex2:
            return ir.FPExp2(op_a);
        case Operation::Lg2:
            return ir.FPLog2(op_a);
        case Operation::Rcp:
            return ir.FPRecip(op_a);
        case Operation::Rsq:
            return ir.FPRecipSqrt(op_a);
        case Operation::Rcp64H:
            throw NotImplementedException("MUFU.RCP64H");
        case Operation::Rsq64H:
            throw NotImplementedException("MUFU.RSQ64H");
        case Operation::Sqrt:
            return ir.FPSqrt(op_a);
        default:
            throw NotImplementedException("Invalid MUFU operation {}", mufu.operation.Value());
        }
    }()};

    if (mufu.sat) {
        value = ir.FPSaturate(value);
    }

    F(mufu.dest_reg, value);
}

} // namespace Shader::Maxwell
