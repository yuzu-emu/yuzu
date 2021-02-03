// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class SpecialRegister : u64 {
    SR_LANEID = 0,
    SR_VIRTCFG = 2,
    SR_VIRTID = 3,
    SR_PM0 = 4,
    SR_PM1 = 5,
    SR_PM2 = 6,
    SR_PM3 = 7,
    SR_PM4 = 8,
    SR_PM5 = 9,
    SR_PM6 = 10,
    SR_PM7 = 11,
    SR_ORDERING_TICKET = 15,
    SR_PRIM_TYPE = 16,
    SR_INVOCATION_ID = 17,
    SR_Y_DIRECTION = 18,
    SR_THREAD_KILL = 19,
    SM_SHADER_TYPE = 20,
    SR_DIRECTCBEWRITEADDRESSLOW = 21,
    SR_DIRECTCBEWRITEADDRESSHIGH = 22,
    SR_DIRECTCBEWRITEENABLE = 23,
    SR_MACHINE_ID_0 = 24,
    SR_MACHINE_ID_1 = 25,
    SR_MACHINE_ID_2 = 26,
    SR_MACHINE_ID_3 = 27,
    SR_AFFINITY = 28,
    SR_INVOCATION_INFO = 29,
    SR_WSCALEFACTOR_XY = 30,
    SR_WSCALEFACTOR_Z = 31,
    SR_TID = 32,
    SR_TID_X = 33,
    SR_TID_Y = 34,
    SR_TID_Z = 35,
    SR_CTAID_X = 37,
    SR_CTAID_Y = 38,
    SR_CTAID_Z = 39,
    SR_NTID = 49,
    SR_CirQueueIncrMinusOne = 50,
    SR_NLATC = 51,
    SR_SWINLO = 57,
    SR_SWINSZ = 58,
    SR_SMEMSZ = 59,
    SR_SMEMBANKS = 60,
    SR_LWINLO = 61,
    SR_LWINSZ = 62,
    SR_LMEMLOSZ = 63,
    SR_LMEMHIOFF = 64,
    SR_EQMASK = 65,
    SR_LTMASK = 66,
    SR_LEMASK = 67,
    SR_GTMASK = 68,
    SR_GEMASK = 69,
    SR_REGALLOC = 70,
    SR_GLOBALERRORSTATUS = 73,
    SR_WARPERRORSTATUS = 75,
    SR_PM_HI0 = 81,
    SR_PM_HI1 = 82,
    SR_PM_HI2 = 83,
    SR_PM_HI3 = 84,
    SR_PM_HI4 = 85,
    SR_PM_HI5 = 86,
    SR_PM_HI6 = 87,
    SR_PM_HI7 = 88,
    SR_CLOCKLO = 89,
    SR_CLOCKHI = 90,
    SR_GLOBALTIMERLO = 91,
    SR_GLOBALTIMERHI = 92,
    SR_HWTASKID = 105,
    SR_CIRCULARQUEUEENTRYINDEX = 106,
    SR_CIRCULARQUEUEENTRYADDRESSLOW = 107,
    SR_CIRCULARQUEUEENTRYADDRESSHIGH = 108,
};

[[nodiscard]] IR::U32 Read(IR::IREmitter& ir, SpecialRegister special_register) {
    switch (special_register) {
    case SpecialRegister::SR_TID_X:
        return ir.LocalInvocationIdX();
    case SpecialRegister::SR_TID_Y:
        return ir.LocalInvocationIdY();
    case SpecialRegister::SR_TID_Z:
        return ir.LocalInvocationIdZ();
    case SpecialRegister::SR_CTAID_X:
        return ir.WorkgroupIdX();
    case SpecialRegister::SR_CTAID_Y:
        return ir.WorkgroupIdY();
    case SpecialRegister::SR_CTAID_Z:
        return ir.WorkgroupIdZ();
    default:
        throw NotImplementedException("S2R special register {}", special_register);
    }
}
} // Anonymous namespace

void TranslatorVisitor::S2R(u64 insn) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<20, 8, SpecialRegister> src_reg;
    } const s2r{insn};

    X(s2r.dest_reg, Read(ir, s2r.src_reg));
}

} // namespace Shader::Maxwell
