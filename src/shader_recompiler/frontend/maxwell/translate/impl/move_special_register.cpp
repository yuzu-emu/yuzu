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
    SR_CLOCK = 1,
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
    SR12 = 12,
    SR13 = 13,
    SR14 = 14,
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
    SR_CTA_PARAM = 36,
    SR_CTAID_X = 37,
    SR_CTAID_Y = 38,
    SR_CTAID_Z = 39,
    SR_NTID = 40,
    SR_CirQueueIncrMinusOne = 41,
    SR_NLATC = 42,
    SR43 = 43,
    SR_SM_SPA_VERSION = 44,
    SR_MULTIPASSSHADERINFO = 45,
    SR_LWINHI = 46,
    SR_SWINHI = 47,
    SR_SWINLO = 48,
    SR_SWINSZ = 49,
    SR_SMEMSZ = 50,
    SR_SMEMBANKS = 51,
    SR_LWINLO = 52,
    SR_LWINSZ = 53,
    SR_LMEMLOSZ = 54,
    SR_LMEMHIOFF = 55,
    SR_EQMASK = 56,
    SR_LTMASK = 57,
    SR_LEMASK = 58,
    SR_GTMASK = 59,
    SR_GEMASK = 60,
    SR_REGALLOC = 61,
    SR_BARRIERALLOC = 62,
    SR63 = 63,
    SR_GLOBALERRORSTATUS = 64,
    SR65 = 65,
    SR_WARPERRORSTATUS = 66,
    SR_WARPERRORSTATUSCLEAR = 67,
    SR68 = 68,
    SR69 = 69,
    SR70 = 70,
    SR71 = 71,
    SR_PM_HI0 = 72,
    SR_PM_HI1 = 73,
    SR_PM_HI2 = 74,
    SR_PM_HI3 = 75,
    SR_PM_HI4 = 76,
    SR_PM_HI5 = 77,
    SR_PM_HI6 = 78,
    SR_PM_HI7 = 79,
    SR_CLOCKLO = 80,
    SR_CLOCKHI = 81,
    SR_GLOBALTIMERLO = 82,
    SR_GLOBALTIMERHI = 83,
    SR84 = 84,
    SR85 = 85,
    SR86 = 86,
    SR87 = 87,
    SR88 = 88,
    SR89 = 89,
    SR90 = 90,
    SR91 = 91,
    SR92 = 92,
    SR93 = 93,
    SR94 = 94,
    SR95 = 95,
    SR_HWTASKID = 96,
    SR_CIRCULARQUEUEENTRYINDEX = 97,
    SR_CIRCULARQUEUEENTRYADDRESSLOW = 98,
    SR_CIRCULARQUEUEENTRYADDRESSHIGH = 99,
};

[[nodiscard]] IR::U32 Read(IR::IREmitter& ir, SpecialRegister special_register) {
    switch (special_register) {
    case SpecialRegister::SR_INVOCATION_ID:
        return ir.InvocationId();
    case SpecialRegister::SR_THREAD_KILL:
        return IR::U32{ir.Select(ir.IsHelperInvocation(), ir.Imm32(-1), ir.Imm32(0))};
    case SpecialRegister::SR_INVOCATION_INFO:
        LOG_WARNING(Shader, "(STUBBED) SR_INVOCATION_INFO");
        return ir.Imm32(0x00ff'0000);
    case SpecialRegister::SR_TID: {
        const IR::Value tid{ir.LocalInvocationId()};
        return ir.BitFieldInsert(ir.BitFieldInsert(IR::U32{ir.CompositeExtract(tid, 0)},
                                                   IR::U32{ir.CompositeExtract(tid, 1)},
                                                   ir.Imm32(16), ir.Imm32(8)),
                                 IR::U32{ir.CompositeExtract(tid, 2)}, ir.Imm32(26), ir.Imm32(6));
    }
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
    case SpecialRegister::SR_WSCALEFACTOR_XY:
        LOG_WARNING(Shader, "(STUBBED) SR_WSCALEFACTOR_XY");
        return ir.Imm32(Common::BitCast<u32>(1.0f));
    case SpecialRegister::SR_WSCALEFACTOR_Z:
        LOG_WARNING(Shader, "(STUBBED) SR_WSCALEFACTOR_Z");
        return ir.Imm32(Common::BitCast<u32>(1.0f));
    case SpecialRegister::SR_LANEID:
        return ir.LaneId();
    case SpecialRegister::SR_EQMASK:
        return ir.SubgroupEqMask();
    case SpecialRegister::SR_LTMASK:
        return ir.SubgroupLtMask();
    case SpecialRegister::SR_LEMASK:
        return ir.SubgroupLeMask();
    case SpecialRegister::SR_GTMASK:
        return ir.SubgroupGtMask();
    case SpecialRegister::SR_GEMASK:
        return ir.SubgroupGeMask();
    case SpecialRegister::SR_Y_DIRECTION:
        return ir.BitCast<IR::U32>(ir.YDirection());
    case SpecialRegister::SR_AFFINITY:
        LOG_WARNING(Shader, "(STUBBED) SR_AFFINITY");
        return ir.Imm32(0); // This is the default value hardware returns.
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
