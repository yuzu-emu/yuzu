// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <bitset>
#include <optional>
#include <tuple>
#include <vector>

#include "common/assert.h"
#include "common/bit_field.h"
#include "common/common_types.h"

namespace Tegra::Shader {

struct Register {
    /// Number of registers
    static constexpr std::size_t NumRegisters = 256;

    /// Register 255 is special cased to always be 0
    static constexpr std::size_t ZeroIndex = 255;

    enum class Size : u64 {
        Byte = 0,
        Short = 1,
        Word = 2,
        Long = 3,
    };

    constexpr Register() = default;

    constexpr Register(u64 value) : value(value) {}

    constexpr operator u64() const {
        return value;
    }

    template <typename T>
    constexpr u64 operator-(const T& oth) const {
        return value - oth;
    }

    template <typename T>
    constexpr u64 operator&(const T& oth) const {
        return value & oth;
    }

    constexpr u64 operator&(const Register& oth) const {
        return value & oth.value;
    }

    constexpr u64 operator~() const {
        return ~value;
    }

    u64 GetSwizzledIndex(u64 elem) const {
        elem = (value + elem) & 3;
        return (value & ~3) + elem;
    }

private:
    u64 value{};
};

enum class AttributeSize : u64 {
    Word = 0,
    DoubleWord = 1,
    TripleWord = 2,
    QuadWord = 3,
};

union Attribute {
    Attribute() = default;

    constexpr explicit Attribute(u64 value) : value(value) {}

    enum class Index : u64 {
        LayerViewportPointSize = 6,
        Position = 7,
        Attribute_0 = 8,
        Attribute_31 = 39,
        FrontColor = 40,
        FrontSecondaryColor = 41,
        BackColor = 42,
        BackSecondaryColor = 43,
        ClipDistances0123 = 44,
        ClipDistances4567 = 45,
        PointCoord = 46,
        // This attribute contains a tuple of (~, ~, InstanceId, VertexId) when inside a vertex
        // shader, and a tuple of (TessCoord.x, TessCoord.y, TessCoord.z, ~) when inside a Tess Eval
        // shader.
        TessCoordInstanceIDVertexID = 47,
        TexCoord_0 = 48,
        TexCoord_7 = 55,
        // This attribute contains a tuple of (Unk, Unk, Unk, gl_FrontFacing) when inside a fragment
        // shader. It is unknown what the other values contain.
        FrontFacing = 63,
    };

    union {
        BitField<20, 10, u64> immediate;
        BitField<22, 2, u64> element;
        BitField<24, 6, Index> index;
        BitField<31, 1, u64> patch;
        BitField<47, 3, AttributeSize> size;

        bool IsPhysical() const {
            return patch == 0 && element == 0 && static_cast<u64>(index.Value()) == 0;
        }
    } fmt20;

    union {
        BitField<30, 2, u64> element;
        BitField<32, 6, Index> index;
    } fmt28;

    BitField<39, 8, u64> reg;
    u64 value{};
};

union Sampler {
    Sampler() = default;

    constexpr explicit Sampler(u64 value) : value(value) {}

    enum class Index : u64 {
        Sampler_0 = 8,
    };

    BitField<36, 13, Index> index;
    u64 value{};
};

union Image {
    Image() = default;

    constexpr explicit Image(u64 value) : value{value} {}

    BitField<36, 13, u64> index;
    u64 value;
};

} // namespace Tegra::Shader

namespace std {

// TODO(bunnei): The below is forbidden by the C++ standard, but works fine. See #330.
template <>
struct make_unsigned<Tegra::Shader::Attribute> {
    using type = Tegra::Shader::Attribute;
};

template <>
struct make_unsigned<Tegra::Shader::Register> {
    using type = Tegra::Shader::Register;
};

} // namespace std

namespace Tegra::Shader {

enum class Pred : u64 {
    UnusedIndex = 0x7,
    NeverExecute = 0xF,
};

enum class PredCondition : u64 {
    F = 0,    // Always false
    LT = 1,   // Ordered less than
    EQ = 2,   // Ordered equal
    LE = 3,   // Ordered less than or equal
    GT = 4,   // Ordered greater than
    NE = 5,   // Ordered not equal
    GE = 6,   // Ordered greater than or equal
    NUM = 7,  // Ordered
    NAN_ = 8, // Unordered
    LTU = 9,  // Unordered less than
    EQU = 10, // Unordered equal
    LEU = 11, // Unordered less than or equal
    GTU = 12, // Unordered greater than
    NEU = 13, // Unordered not equal
    GEU = 14, // Unordered greater than or equal
    T = 15,   // Always true
};

enum class PredOperation : u64 {
    And = 0,
    Or = 1,
    Xor = 2,
};

enum class LogicOperation : u64 {
    And = 0,
    Or = 1,
    Xor = 2,
    PassB = 3,
};

enum class SubOp : u64 {
    Cos = 0x0,
    Sin = 0x1,
    Ex2 = 0x2,
    Lg2 = 0x3,
    Rcp = 0x4,
    Rsq = 0x5,
    Sqrt = 0x8,
};

enum class F2iRoundingOp : u64 {
    RoundEven = 0,
    Floor = 1,
    Ceil = 2,
    Trunc = 3,
};

enum class F2fRoundingOp : u64 {
    None = 0,
    Pass = 3,
    Round = 8,
    Floor = 9,
    Ceil = 10,
    Trunc = 11,
};

enum class AtomicOp : u64 {
    Add = 0,
    Min = 1,
    Max = 2,
    Inc = 3,
    Dec = 4,
    And = 5,
    Or = 6,
    Xor = 7,
    Exch = 8,
    SafeAdd = 10,
};

enum class GlobalAtomicType : u64 {
    U32 = 0,
    S32 = 1,
    U64 = 2,
    F32_FTZ_RN = 3,
    F16x2_FTZ_RN = 4,
    S64 = 5,
};

enum class UniformType : u64 {
    UnsignedByte = 0,
    SignedByte = 1,
    UnsignedShort = 2,
    SignedShort = 3,
    Single = 4,
    Double = 5,
    Quad = 6,
    UnsignedQuad = 7,
};

enum class StoreType : u64 {
    Unsigned8 = 0,
    Signed8 = 1,
    Unsigned16 = 2,
    Signed16 = 3,
    Bits32 = 4,
    Bits64 = 5,
    Bits128 = 6,
};

enum class AtomicType : u64 {
    U32 = 0,
    S32 = 1,
    U64 = 2,
    S64 = 3,
};

enum class IMinMaxExchange : u64 {
    None = 0,
    XLo = 1,
    XMed = 2,
    XHi = 3,
};

enum class VideoType : u64 {
    Size16_Low = 0,
    Size16_High = 1,
    Size32 = 2,
    Invalid = 3,
};

enum class VmadShr : u64 {
    Shr7 = 1,
    Shr15 = 2,
};

enum class VmnmxType : u64 {
    Bits8,
    Bits16,
    Bits32,
};

enum class VmnmxOperation : u64 {
    Mrg_16H = 0,
    Mrg_16L = 1,
    Mrg_8B0 = 2,
    Mrg_8B2 = 3,
    Acc = 4,
    Min = 5,
    Max = 6,
    Nop = 7,
};

enum class XmadMode : u64 {
    None = 0,
    CLo = 1,
    CHi = 2,
    CSfu = 3,
    CBcc = 4,
};

enum class IAdd3Mode : u64 {
    None = 0,
    RightShift = 1,
    LeftShift = 2,
};

enum class IAdd3Height : u64 {
    None = 0,
    LowerHalfWord = 1,
    UpperHalfWord = 2,
};

enum class FlowCondition : u64 {
    Always = 0xF,
    Fcsm_Tr = 0x1C, // TODO(bunnei): What is this used for?
};

enum class ConditionCode : u64 {
    F = 0,
    LT = 1,
    EQ = 2,
    LE = 3,
    GT = 4,
    NE = 5,
    GE = 6,
    Num = 7,
    Nan = 8,
    LTU = 9,
    EQU = 10,
    LEU = 11,
    GTU = 12,
    NEU = 13,
    GEU = 14,
    T = 15,
    OFF = 16,
    LO = 17,
    SFF = 18,
    LS = 19,
    HI = 20,
    SFT = 21,
    HS = 22,
    OFT = 23,
    CSM_TA = 24,
    CSM_TR = 25,
    CSM_MX = 26,
    FCSM_TA = 27,
    FCSM_TR = 28,
    FCSM_MX = 29,
    RLE = 30,
    RGT = 31,
};

enum class PredicateResultMode : u64 {
    None = 0x0,
    NotZero = 0x3,
};

enum class TextureType : u64 {
    Texture1D = 0,
    Texture2D = 1,
    Texture3D = 2,
    TextureCube = 3,
};

enum class TextureQueryType : u64 {
    Dimension = 1,
    TextureType = 2,
    SamplePosition = 5,
    Filter = 16,
    LevelOfDetail = 18,
    Wrap = 20,
    BorderColor = 22,
};

enum class TextureProcessMode : u64 {
    None = 0,
    LZ = 1,  // Load LOD of zero.
    LB = 2,  // Load Bias.
    LL = 3,  // Load LOD.
    LBA = 6, // Load Bias. The A is unknown, does not appear to differ with LB.
    LLA = 7  // Load LOD. The A is unknown, does not appear to differ with LL.
};

enum class TextureMiscMode : u64 {
    DC,
    AOFFI, // Uses Offset
    NDV,
    NODEP,
    MZ,
    PTP,
};

enum class SurfaceDataMode : u64 {
    P = 0,
    D_BA = 1,
};

enum class OutOfBoundsStore : u64 {
    Ignore = 0,
    Clamp = 1,
    Trap = 2,
};

enum class ImageType : u64 {
    Texture1D = 0,
    TextureBuffer = 1,
    Texture1DArray = 2,
    Texture2D = 3,
    Texture2DArray = 4,
    Texture3D = 5,
};

enum class IsberdMode : u64 {
    None = 0,
    Patch = 1,
    Prim = 2,
    Attr = 3,
};

enum class IsberdShift : u64 { None = 0, U16 = 1, B32 = 2 };

enum class MembarType : u64 {
    CTA = 0,
    GL = 1,
    SYS = 2,
    VC = 3,
};

enum class MembarUnknown : u64 { Default = 0, IVALLD = 1, IVALLT = 2, IVALLTD = 3 };

enum class HalfType : u64 {
    H0_H1 = 0,
    F32 = 1,
    H0_H0 = 2,
    H1_H1 = 3,
};

enum class HalfMerge : u64 {
    H0_H1 = 0,
    F32 = 1,
    Mrg_H0 = 2,
    Mrg_H1 = 3,
};

enum class HalfPrecision : u64 {
    None = 0,
    FTZ = 1,
    FMZ = 2,
};

enum class R2pMode : u64 {
    Pr = 0,
    Cc = 1,
};

enum class IpaInterpMode : u64 {
    Pass = 0,
    Multiply = 1,
    Constant = 2,
    Sc = 3,
};

enum class IpaSampleMode : u64 {
    Default = 0,
    Centroid = 1,
    Offset = 2,
};

enum class LmemLoadCacheManagement : u64 {
    Default = 0,
    LU = 1,
    CI = 2,
    CV = 3,
};

enum class StoreCacheManagement : u64 {
    Default = 0,
    CG = 1,
    CS = 2,
    WT = 3,
};

struct IpaMode {
    IpaInterpMode interpolation_mode;
    IpaSampleMode sampling_mode;

    auto operator<=>(const IpaMode&) const = default;
};

enum class SystemVariable : u64 {
    LaneId = 0x00,
    VirtCfg = 0x02,
    VirtId = 0x03,
    Pm0 = 0x04,
    Pm1 = 0x05,
    Pm2 = 0x06,
    Pm3 = 0x07,
    Pm4 = 0x08,
    Pm5 = 0x09,
    Pm6 = 0x0a,
    Pm7 = 0x0b,
    OrderingTicket = 0x0f,
    PrimType = 0x10,
    InvocationId = 0x11,
    Ydirection = 0x12,
    ThreadKill = 0x13,
    ShaderType = 0x14,
    DirectBeWriteAddressLow = 0x15,
    DirectBeWriteAddressHigh = 0x16,
    DirectBeWriteEnabled = 0x17,
    MachineId0 = 0x18,
    MachineId1 = 0x19,
    MachineId2 = 0x1a,
    MachineId3 = 0x1b,
    Affinity = 0x1c,
    InvocationInfo = 0x1d,
    WscaleFactorXY = 0x1e,
    WscaleFactorZ = 0x1f,
    Tid = 0x20,
    TidX = 0x21,
    TidY = 0x22,
    TidZ = 0x23,
    CtaParam = 0x24,
    CtaIdX = 0x25,
    CtaIdY = 0x26,
    CtaIdZ = 0x27,
    NtId = 0x28,
    CirQueueIncrMinusOne = 0x29,
    Nlatc = 0x2a,
    SmSpaVersion = 0x2c,
    MultiPassShaderInfo = 0x2d,
    LwinHi = 0x2e,
    SwinHi = 0x2f,
    SwinLo = 0x30,
    SwinSz = 0x31,
    SmemSz = 0x32,
    SmemBanks = 0x33,
    LwinLo = 0x34,
    LwinSz = 0x35,
    LmemLosz = 0x36,
    LmemHioff = 0x37,
    EqMask = 0x38,
    LtMask = 0x39,
    LeMask = 0x3a,
    GtMask = 0x3b,
    GeMask = 0x3c,
    RegAlloc = 0x3d,
    CtxAddr = 0x3e,      // .fmask = F_SM50
    BarrierAlloc = 0x3e, // .fmask = F_SM60
    GlobalErrorStatus = 0x40,
    WarpErrorStatus = 0x42,
    WarpErrorStatusClear = 0x43,
    PmHi0 = 0x48,
    PmHi1 = 0x49,
    PmHi2 = 0x4a,
    PmHi3 = 0x4b,
    PmHi4 = 0x4c,
    PmHi5 = 0x4d,
    PmHi6 = 0x4e,
    PmHi7 = 0x4f,
    ClockLo = 0x50,
    ClockHi = 0x51,
    GlobalTimerLo = 0x52,
    GlobalTimerHi = 0x53,
    HwTaskId = 0x60,
    CircularQueueEntryIndex = 0x61,
    CircularQueueEntryAddressLow = 0x62,
    CircularQueueEntryAddressHigh = 0x63,
};

enum class PhysicalAttributeDirection : u64 {
    Input = 0,
    Output = 1,
};

enum class VoteOperation : u64 {
    All = 0, // allThreadsNV
    Any = 1, // anyThreadNV
    Eq = 2,  // allThreadsEqualNV
};

enum class ImageAtomicOperationType : u64 {
    U32 = 0,
    S32 = 1,
    U64 = 2,
    F32 = 3,
    S64 = 5,
    SD32 = 6,
    SD64 = 7,
};

enum class ImageAtomicOperation : u64 {
    Add = 0,
    Min = 1,
    Max = 2,
    Inc = 3,
    Dec = 4,
    And = 5,
    Or = 6,
    Xor = 7,
    Exch = 8,
};

enum class ShuffleOperation : u64 {
    Idx = 0,  // shuffleNV
    Up = 1,   // shuffleUpNV
    Down = 2, // shuffleDownNV
    Bfly = 3, // shuffleXorNV
};

enum class ShfType : u64 {
    Bits32 = 0,
    U64 = 2,
    S64 = 3,
};

enum class ShfXmode : u64 {
    None = 0,
    HI = 1,
    X = 2,
    XHI = 3,
};

union Instruction {
    constexpr Instruction& operator=(const Instruction& instr) {
        value = instr.value;
        return *this;
    }

    constexpr Instruction(u64 value) : value{value} {}
    constexpr Instruction(const Instruction& instr) : value(instr.value) {}

    constexpr bool Bit(u64 offset) const {
        return ((value >> offset) & 1) != 0;
    }

    BitField<0, 8, Register> gpr0;
    BitField<8, 8, Register> gpr8;
    union {
        BitField<16, 4, Pred> full_pred;
        BitField<16, 3, u64> pred_index;
    } pred;
    BitField<19, 1, u64> negate_pred;
    BitField<20, 8, Register> gpr20;
    BitField<20, 4, SubOp> sub_op;
    BitField<28, 8, Register> gpr28;
    BitField<39, 8, Register> gpr39;
    BitField<48, 16, u64> opcode;

    union {
        BitField<8, 5, ConditionCode> cc;
        BitField<13, 1, u64> trigger;
    } nop;

    union {
        BitField<48, 2, VoteOperation> operation;
        BitField<45, 3, u64> dest_pred;
        BitField<39, 3, u64> value;
        BitField<42, 1, u64> negate_value;
    } vote;

    union {
        BitField<30, 2, ShuffleOperation> operation;
        BitField<48, 3, u64> pred48;
        BitField<28, 1, u64> is_index_imm;
        BitField<29, 1, u64> is_mask_imm;
        BitField<20, 5, u64> index_imm;
        BitField<34, 13, u64> mask_imm;
    } shfl;

    union {
        BitField<44, 1, u64> ftz;
        BitField<39, 2, u64> tab5cb8_2;
        BitField<38, 1, u64> ndv;
        BitField<47, 1, u64> cc;
        BitField<28, 8, u64> swizzle;
    } fswzadd;

    union {
        BitField<8, 8, Register> gpr;
        BitField<20, 24, s64> offset;
    } gmem;

    union {
        BitField<20, 16, u64> imm20_16;
        BitField<20, 19, u64> imm20_19;
        BitField<20, 32, s64> imm20_32;
        BitField<45, 1, u64> negate_b;
        BitField<46, 1, u64> abs_a;
        BitField<48, 1, u64> negate_a;
        BitField<49, 1, u64> abs_b;
        BitField<50, 1, u64> saturate_d;
        BitField<56, 1, u64> negate_imm;

        union {
            BitField<39, 3, u64> pred;
            BitField<42, 1, u64> negate_pred;
        } fmnmx;

        union {
            BitField<39, 1, u64> invert_a;
            BitField<40, 1, u64> invert_b;
            BitField<41, 2, LogicOperation> operation;
            BitField<44, 2, PredicateResultMode> pred_result_mode;
            BitField<48, 3, Pred> pred48;
        } lop;

        union {
            BitField<53, 2, LogicOperation> operation;
            BitField<55, 1, u64> invert_a;
            BitField<56, 1, u64> invert_b;
        } lop32i;

        union {
            BitField<28, 8, u64> imm_lut28;
            BitField<48, 8, u64> imm_lut48;

            u32 GetImmLut28() const {
                return static_cast<u32>(imm_lut28);
            }

            u32 GetImmLut48() const {
                return static_cast<u32>(imm_lut48);
            }
        } lop3;

        u16 GetImm20_16() const {
            return static_cast<u16>(imm20_16);
        }

        u32 GetImm20_19() const {
            u32 imm{static_cast<u32>(imm20_19)};
            imm <<= 12;
            imm |= negate_imm ? 0x80000000 : 0;
            return imm;
        }

        u32 GetImm20_32() const {
            return static_cast<u32>(imm20_32);
        }

        s32 GetSignedImm20_20() const {
            u32 immediate = static_cast<u32>(imm20_19 | (negate_imm << 19));
            // Sign extend the 20-bit value.
            u32 mask = 1U << (20 - 1);
            return static_cast<s32>((immediate ^ mask) - mask);
        }
    } alu;

    union {
        BitField<38, 1, u64> idx;
        BitField<51, 1, u64> saturate;
        BitField<52, 2, IpaSampleMode> sample_mode;
        BitField<54, 2, IpaInterpMode> interp_mode;
    } ipa;

    union {
        BitField<39, 2, u64> tab5cb8_2;
        BitField<41, 3, u64> postfactor;
        BitField<44, 2, u64> tab5c68_0;
        BitField<48, 1, u64> negate_b;
    } fmul;

    union {
        BitField<55, 1, u64> saturate;
    } fmul32;

    union {
        BitField<52, 1, u64> generates_cc;
    } op_32;

    union {
        BitField<48, 1, u64> is_signed;
    } shift;

    union {
        BitField<39, 1, u64> wrap;
    } shr;

    union {
        BitField<37, 2, ShfType> type;
        BitField<48, 2, ShfXmode> xmode;
        BitField<50, 1, u64> wrap;
        BitField<20, 6, u64> immediate;
    } shf;

    union {
        BitField<39, 5, u64> shift_amount;
        BitField<48, 1, u64> negate_b;
        BitField<49, 1, u64> negate_a;
    } alu_integer;

    union {
        BitField<43, 1, u64> x;
    } iadd;

    union {
        BitField<39, 1, u64> ftz;
        BitField<32, 1, u64> saturate;
        BitField<49, 2, HalfMerge> merge;

        BitField<44, 1, u64> abs_a;
        BitField<47, 2, HalfType> type_a;

        BitField<30, 1, u64> abs_b;
        BitField<28, 2, HalfType> type_b;

        BitField<35, 2, HalfType> type_c;
    } alu_half;

    union {
        BitField<39, 2, HalfPrecision> precision;
        BitField<39, 1, u64> ftz;
        BitField<52, 1, u64> saturate;
        BitField<49, 2, HalfMerge> merge;

        BitField<43, 1, u64> negate_a;
        BitField<44, 1, u64> abs_a;
        BitField<47, 2, HalfType> type_a;
    } alu_half_imm;

    union {
        BitField<29, 1, u64> first_negate;
        BitField<20, 9, u64> first;

        BitField<56, 1, u64> second_negate;
        BitField<30, 9, u64> second;

        u32 PackImmediates() const {
            // Immediates are half floats shifted.
            constexpr u32 imm_shift = 6;
            return static_cast<u32>((first << imm_shift) | (second << (16 + imm_shift)));
        }
    } half_imm;

    union {
        union {
            BitField<37, 2, HalfPrecision> precision;
            BitField<32, 1, u64> saturate;

            BitField<31, 1, u64> negate_b;
            BitField<30, 1, u64> negate_c;
            BitField<35, 2, HalfType> type_c;
        } rr;

        BitField<57, 2, HalfPrecision> precision;
        BitField<52, 1, u64> saturate;

        BitField<49, 2, HalfMerge> merge;

        BitField<47, 2, HalfType> type_a;

        BitField<56, 1, u64> negate_b;
        BitField<28, 2, HalfType> type_b;

        BitField<51, 1, u64> negate_c;
        BitField<53, 2, HalfType> type_reg39;
    } hfma2;

    union {
        BitField<40, 1, u64> invert;
    } popc;

    union {
        BitField<41, 1, u64> sh;
        BitField<40, 1, u64> invert;
        BitField<48, 1, u64> is_signed;
    } flo;

    union {
        BitField<39, 3, u64> pred;
        BitField<42, 1, u64> neg_pred;
    } sel;

    union {
        BitField<39, 3, u64> pred;
        BitField<42, 1, u64> negate_pred;
        BitField<43, 2, IMinMaxExchange> exchange;
        BitField<48, 1, u64> is_signed;
    } imnmx;

    union {
        BitField<31, 2, IAdd3Height> height_c;
        BitField<33, 2, IAdd3Height> height_b;
        BitField<35, 2, IAdd3Height> height_a;
        BitField<37, 2, IAdd3Mode> mode;
        BitField<49, 1, u64> neg_c;
        BitField<50, 1, u64> neg_b;
        BitField<51, 1, u64> neg_a;
    } iadd3;

    union {
        BitField<54, 1, u64> saturate;
        BitField<56, 1, u64> negate_a;
    } iadd32i;

    union {
        BitField<53, 1, u64> negate_b;
        BitField<54, 1, u64> abs_a;
        BitField<56, 1, u64> negate_a;
        BitField<57, 1, u64> abs_b;
    } fadd32i;

    union {
        BitField<40, 1, u64> brev;
        BitField<47, 1, u64> rd_cc;
        BitField<48, 1, u64> is_signed;
    } bfe;

    union {
        BitField<48, 3, u64> pred48;

        union {
            BitField<20, 20, u64> entry_a;
            BitField<39, 5, u64> entry_b;
            BitField<45, 1, u64> neg;
            BitField<46, 1, u64> uses_cc;
        } imm;

        union {
            BitField<20, 14, u64> cb_index;
            BitField<34, 5, u64> cb_offset;
            BitField<56, 1, u64> neg;
            BitField<57, 1, u64> uses_cc;
        } hi;

        union {
            BitField<20, 14, u64> cb_index;
            BitField<34, 5, u64> cb_offset;
            BitField<39, 5, u64> entry_a;
            BitField<45, 1, u64> neg;
            BitField<46, 1, u64> uses_cc;
        } rz;

        union {
            BitField<39, 5, u64> entry_a;
            BitField<45, 1, u64> neg;
            BitField<46, 1, u64> uses_cc;
        } r1;

        union {
            BitField<28, 8, u64> entry_a;
            BitField<37, 1, u64> neg;
            BitField<38, 1, u64> uses_cc;
        } r2;

    } lea;

    union {
        BitField<0, 5, FlowCondition> cond;
    } flow;

    union {
        BitField<47, 1, u64> cc;
        BitField<48, 1, u64> negate_b;
        BitField<49, 1, u64> negate_c;
        BitField<51, 2, u64> tab5980_1;
        BitField<53, 2, u64> tab5980_0;
    } ffma;

    union {
        BitField<48, 3, UniformType> type;
        BitField<44, 2, u64> unknown;
    } ld_c;

    union {
        BitField<48, 3, StoreType> type;
    } ldst_sl;

    union {
        BitField<44, 2, u64> unknown;
    } ld_l;

    union {
        BitField<44, 2, StoreCacheManagement> cache_management;
    } st_l;

    union {
        BitField<48, 3, UniformType> type;
        BitField<46, 2, u64> cache_mode;
    } ldg;

    union {
        BitField<48, 3, UniformType> type;
        BitField<46, 2, u64> cache_mode;
    } stg;

    union {
        BitField<23, 3, AtomicOp> operation;
        BitField<48, 1, u64> extended;
        BitField<20, 3, GlobalAtomicType> type;
    } red;

    union {
        BitField<52, 4, AtomicOp> operation;
        BitField<49, 3, GlobalAtomicType> type;
        BitField<28, 20, s64> offset;
    } atom;

    union {
        BitField<52, 4, AtomicOp> operation;
        BitField<28, 2, AtomicType> type;
        BitField<30, 22, s64> offset;

        s32 GetImmediateOffset() const {
            return static_cast<s32>(offset << 2);
        }
    } atoms;

    union {
        BitField<32, 1, PhysicalAttributeDirection> direction;
        BitField<47, 3, AttributeSize> size;
        BitField<20, 11, u64> address;
    } al2p;

    union {
        BitField<53, 3, UniformType> type;
        BitField<52, 1, u64> extended;
    } generic;

    union {
        BitField<0, 3, u64> pred0;
        BitField<3, 3, u64> pred3;
        BitField<6, 1, u64> neg_b;
        BitField<7, 1, u64> abs_a;
        BitField<39, 3, u64> pred39;
        BitField<42, 1, u64> neg_pred;
        BitField<43, 1, u64> neg_a;
        BitField<44, 1, u64> abs_b;
        BitField<45, 2, PredOperation> op;
        BitField<47, 1, u64> ftz;
        BitField<48, 4, PredCondition> cond;
    } fsetp;

    union {
        BitField<0, 3, u64> pred0;
        BitField<3, 3, u64> pred3;
        BitField<39, 3, u64> pred39;
        BitField<42, 1, u64> neg_pred;
        BitField<45, 2, PredOperation> op;
        BitField<48, 1, u64> is_signed;
        BitField<49, 3, PredCondition> cond;
    } isetp;

    union {
        BitField<48, 1, u64> is_signed;
        BitField<49, 3, PredCondition> cond;
    } icmp;

    union {
        BitField<0, 3, u64> pred0;
        BitField<3, 3, u64> pred3;
        BitField<12, 3, u64> pred12;
        BitField<15, 1, u64> neg_pred12;
        BitField<24, 2, PredOperation> cond;
        BitField<29, 3, u64> pred29;
        BitField<32, 1, u64> neg_pred29;
        BitField<39, 3, u64> pred39;
        BitField<42, 1, u64> neg_pred39;
        BitField<45, 2, PredOperation> op;
    } psetp;

    union {
        BitField<43, 4, PredCondition> cond;
        BitField<45, 2, PredOperation> op;
        BitField<3, 3, u64> pred3;
        BitField<0, 3, u64> pred0;
        BitField<39, 3, u64> pred39;
    } vsetp;

    union {
        BitField<12, 3, u64> pred12;
        BitField<15, 1, u64> neg_pred12;
        BitField<24, 2, PredOperation> cond;
        BitField<29, 3, u64> pred29;
        BitField<32, 1, u64> neg_pred29;
        BitField<39, 3, u64> pred39;
        BitField<42, 1, u64> neg_pred39;
        BitField<44, 1, u64> bf;
        BitField<45, 2, PredOperation> op;
    } pset;

    union {
        BitField<0, 3, u64> pred0;
        BitField<3, 3, u64> pred3;
        BitField<8, 5, ConditionCode> cc; // flag in cc
        BitField<39, 3, u64> pred39;
        BitField<42, 1, u64> neg_pred39;
        BitField<45, 4, PredOperation> op; // op with pred39
    } csetp;

    union {
        BitField<6, 1, u64> ftz;
        BitField<45, 2, PredOperation> op;
        BitField<3, 3, u64> pred3;
        BitField<0, 3, u64> pred0;
        BitField<43, 1, u64> negate_a;
        BitField<44, 1, u64> abs_a;
        BitField<47, 2, HalfType> type_a;
        union {
            BitField<35, 4, PredCondition> cond;
            BitField<49, 1, u64> h_and;
            BitField<31, 1, u64> negate_b;
            BitField<30, 1, u64> abs_b;
            BitField<28, 2, HalfType> type_b;
        } reg;
        union {
            BitField<56, 1, u64> negate_b;
            BitField<54, 1, u64> abs_b;
        } cbuf;
        union {
            BitField<49, 4, PredCondition> cond;
            BitField<53, 1, u64> h_and;
        } cbuf_and_imm;
        BitField<42, 1, u64> neg_pred;
        BitField<39, 3, u64> pred39;
    } hsetp2;

    union {
        BitField<40, 1, R2pMode> mode;
        BitField<41, 2, u64> byte;
        BitField<20, 7, u64> immediate_mask;
    } p2r_r2p;

    union {
        BitField<39, 3, u64> pred39;
        BitField<42, 1, u64> neg_pred;
        BitField<43, 1, u64> neg_a;
        BitField<44, 1, u64> abs_b;
        BitField<45, 2, PredOperation> op;
        BitField<48, 4, PredCondition> cond;
        BitField<52, 1, u64> bf;
        BitField<53, 1, u64> neg_b;
        BitField<54, 1, u64> abs_a;
        BitField<55, 1, u64> ftz;
    } fset;

    union {
        BitField<47, 1, u64> ftz;
        BitField<48, 4, PredCondition> cond;
    } fcmp;

    union {
        BitField<49, 1, u64> bf;
        BitField<35, 3, PredCondition> cond;
        BitField<50, 1, u64> ftz;
        BitField<45, 2, PredOperation> op;
        BitField<43, 1, u64> negate_a;
        BitField<44, 1, u64> abs_a;
        BitField<47, 2, HalfType> type_a;
        BitField<31, 1, u64> negate_b;
        BitField<30, 1, u64> abs_b;
        BitField<28, 2, HalfType> type_b;
        BitField<42, 1, u64> neg_pred;
        BitField<39, 3, u64> pred39;
    } hset2;

    union {
        BitField<39, 3, u64> pred39;
        BitField<42, 1, u64> neg_pred;
        BitField<44, 1, u64> bf;
        BitField<45, 2, PredOperation> op;
        BitField<48, 1, u64> is_signed;
        BitField<49, 3, PredCondition> cond;
    } iset;

    union {
        BitField<45, 1, u64> negate_a;
        BitField<49, 1, u64> abs_a;
        BitField<10, 2, Register::Size> src_size;
        BitField<13, 1, u64> is_input_signed;
        BitField<8, 2, Register::Size> dst_size;
        BitField<12, 1, u64> is_output_signed;

        union {
            BitField<39, 2, u64> tab5cb8_2;
        } i2f;

        union {
            BitField<39, 2, F2iRoundingOp> rounding;
        } f2i;

        union {
            BitField<39, 4, u64> rounding;
            // H0, H1 extract for F16 missing
            BitField<41, 1, u64> selector; // Guessed as some games set it, TODO: reverse this value
            F2fRoundingOp GetRoundingMode() const {
                constexpr u64 rounding_mask = 0x0B;
                return static_cast<F2fRoundingOp>(rounding.Value() & rounding_mask);
            }
        } f2f;

        union {
            BitField<41, 2, u64> selector;
        } int_src;

        union {
            BitField<41, 1, u64> selector;
        } float_src;
    } conversion;

    union {
        BitField<28, 1, u64> array;
        BitField<29, 2, TextureType> texture_type;
        BitField<31, 4, u64> component_mask;
        BitField<49, 1, u64> nodep_flag;
        BitField<50, 1, u64> dc_flag;
        BitField<54, 1, u64> aoffi_flag;
        BitField<55, 3, TextureProcessMode> process_mode;

        bool IsComponentEnabled(std::size_t component) const {
            return ((1ull << component) & component_mask) != 0;
        }

        TextureProcessMode GetTextureProcessMode() const {
            return process_mode;
        }

        bool UsesMiscMode(TextureMiscMode mode) const {
            switch (mode) {
            case TextureMiscMode::DC:
                return dc_flag != 0;
            case TextureMiscMode::NODEP:
                return nodep_flag != 0;
            case TextureMiscMode::AOFFI:
                return aoffi_flag != 0;
            default:
                break;
            }
            return false;
        }
    } tex;

    union {
        BitField<28, 1, u64> array;
        BitField<29, 2, TextureType> texture_type;
        BitField<31, 4, u64> component_mask;
        BitField<49, 1, u64> nodep_flag;
        BitField<50, 1, u64> dc_flag;
        BitField<36, 1, u64> aoffi_flag;
        BitField<37, 3, TextureProcessMode> process_mode;

        bool IsComponentEnabled(std::size_t component) const {
            return ((1ULL << component) & component_mask) != 0;
        }

        TextureProcessMode GetTextureProcessMode() const {
            return process_mode;
        }

        bool UsesMiscMode(TextureMiscMode mode) const {
            switch (mode) {
            case TextureMiscMode::DC:
                return dc_flag != 0;
            case TextureMiscMode::NODEP:
                return nodep_flag != 0;
            case TextureMiscMode::AOFFI:
                return aoffi_flag != 0;
            default:
                break;
            }
            return false;
        }
    } tex_b;

    union {
        BitField<22, 6, TextureQueryType> query_type;
        BitField<31, 4, u64> component_mask;
        BitField<49, 1, u64> nodep_flag;

        bool UsesMiscMode(TextureMiscMode mode) const {
            switch (mode) {
            case TextureMiscMode::NODEP:
                return nodep_flag != 0;
            default:
                break;
            }
            return false;
        }

        bool IsComponentEnabled(std::size_t component) const {
            return ((1ULL << component) & component_mask) != 0;
        }
    } txq;

    union {
        BitField<28, 1, u64> array;
        BitField<29, 2, TextureType> texture_type;
        BitField<31, 4, u64> component_mask;
        BitField<35, 1, u64> ndv_flag;
        BitField<49, 1, u64> nodep_flag;

        bool IsComponentEnabled(std::size_t component) const {
            return ((1ull << component) & component_mask) != 0;
        }

        bool UsesMiscMode(TextureMiscMode mode) const {
            switch (mode) {
            case TextureMiscMode::NDV:
                return (ndv_flag != 0);
            case TextureMiscMode::NODEP:
                return (nodep_flag != 0);
            default:
                break;
            }
            return false;
        }
    } tmml;

    union {
        BitField<28, 1, u64> array;
        BitField<29, 2, TextureType> texture_type;
        BitField<35, 1, u64> ndv_flag;
        BitField<49, 1, u64> nodep_flag;
        BitField<50, 1, u64> dc_flag;
        BitField<54, 2, u64> offset_mode;
        BitField<56, 2, u64> component;

        bool UsesMiscMode(TextureMiscMode mode) const {
            switch (mode) {
            case TextureMiscMode::NDV:
                return ndv_flag != 0;
            case TextureMiscMode::NODEP:
                return nodep_flag != 0;
            case TextureMiscMode::DC:
                return dc_flag != 0;
            case TextureMiscMode::AOFFI:
                return offset_mode == 1;
            case TextureMiscMode::PTP:
                return offset_mode == 2;
            default:
                break;
            }
            return false;
        }
    } tld4;

    union {
        BitField<35, 1, u64> ndv_flag;
        BitField<49, 1, u64> nodep_flag;
        BitField<50, 1, u64> dc_flag;
        BitField<33, 2, u64> offset_mode;
        BitField<37, 2, u64> component;

        bool UsesMiscMode(TextureMiscMode mode) const {
            switch (mode) {
            case TextureMiscMode::NDV:
                return ndv_flag != 0;
            case TextureMiscMode::NODEP:
                return nodep_flag != 0;
            case TextureMiscMode::DC:
                return dc_flag != 0;
            case TextureMiscMode::AOFFI:
                return offset_mode == 1;
            case TextureMiscMode::PTP:
                return offset_mode == 2;
            default:
                break;
            }
            return false;
        }
    } tld4_b;

    union {
        BitField<49, 1, u64> nodep_flag;
        BitField<50, 1, u64> dc_flag;
        BitField<51, 1, u64> aoffi_flag;
        BitField<52, 2, u64> component;
        BitField<55, 1, u64> fp16_flag;

        bool UsesMiscMode(TextureMiscMode mode) const {
            switch (mode) {
            case TextureMiscMode::DC:
                return dc_flag != 0;
            case TextureMiscMode::NODEP:
                return nodep_flag != 0;
            case TextureMiscMode::AOFFI:
                return aoffi_flag != 0;
            default:
                break;
            }
            return false;
        }
    } tld4s;

    union {
        BitField<0, 8, Register> gpr0;
        BitField<28, 8, Register> gpr28;
        BitField<49, 1, u64> nodep_flag;
        BitField<50, 3, u64> component_mask_selector;
        BitField<53, 4, u64> texture_info;
        BitField<59, 1, u64> fp32_flag;

        TextureType GetTextureType() const {
            // The TEXS instruction has a weird encoding for the texture type.
            if (texture_info == 0)
                return TextureType::Texture1D;
            if (texture_info >= 1 && texture_info <= 9)
                return TextureType::Texture2D;
            if (texture_info >= 10 && texture_info <= 11)
                return TextureType::Texture3D;
            if (texture_info >= 12 && texture_info <= 13)
                return TextureType::TextureCube;

            LOG_CRITICAL(HW_GPU, "Unhandled texture_info: {}",
                         static_cast<u32>(texture_info.Value()));
            UNREACHABLE();
            return TextureType::Texture1D;
        }

        TextureProcessMode GetTextureProcessMode() const {
            switch (texture_info) {
            case 0:
            case 2:
            case 6:
            case 8:
            case 9:
            case 11:
                return TextureProcessMode::LZ;
            case 3:
            case 5:
            case 13:
                return TextureProcessMode::LL;
            default:
                break;
            }
            return TextureProcessMode::None;
        }

        bool UsesMiscMode(TextureMiscMode mode) const {
            switch (mode) {
            case TextureMiscMode::DC:
                return (texture_info >= 4 && texture_info <= 6) || texture_info == 9;
            case TextureMiscMode::NODEP:
                return nodep_flag != 0;
            default:
                break;
            }
            return false;
        }

        bool IsArrayTexture() const {
            // TEXS only supports Texture2D arrays.
            return texture_info >= 7 && texture_info <= 9;
        }

        bool HasTwoDestinations() const {
            return gpr28.Value() != Register::ZeroIndex;
        }

        bool IsComponentEnabled(std::size_t component) const {
            static constexpr std::array<std::array<u32, 8>, 4> mask_lut{{
                {},
                {0x1, 0x2, 0x4, 0x8, 0x3, 0x9, 0xa, 0xc},
                {0x1, 0x2, 0x4, 0x8, 0x3, 0x9, 0xa, 0xc},
                {0x7, 0xb, 0xd, 0xe, 0xf},
            }};

            std::size_t index{gpr0.Value() != Register::ZeroIndex ? 1U : 0U};
            index |= gpr28.Value() != Register::ZeroIndex ? 2 : 0;

            u32 mask = mask_lut[index][component_mask_selector];
            // A mask of 0 means this instruction uses an unimplemented mask.
            ASSERT(mask != 0);
            return ((1ull << component) & mask) != 0;
        }
    } texs;

    union {
        BitField<28, 1, u64> is_array;
        BitField<29, 2, TextureType> texture_type;
        BitField<35, 1, u64> aoffi;
        BitField<49, 1, u64> nodep_flag;
        BitField<50, 1, u64> ms; // Multisample?
        BitField<54, 1, u64> cl;
        BitField<55, 1, u64> process_mode;

        TextureProcessMode GetTextureProcessMode() const {
            return process_mode == 0 ? TextureProcessMode::LZ : TextureProcessMode::LL;
        }
    } tld;

    union {
        BitField<49, 1, u64> nodep_flag;
        BitField<53, 4, u64> texture_info;
        BitField<59, 1, u64> fp32_flag;

        TextureType GetTextureType() const {
            // The TLDS instruction has a weird encoding for the texture type.
            if (texture_info <= 1) {
                return TextureType::Texture1D;
            }
            if (texture_info == 2 || texture_info == 8 || texture_info == 12 ||
                (texture_info >= 4 && texture_info <= 6)) {
                return TextureType::Texture2D;
            }
            if (texture_info == 7) {
                return TextureType::Texture3D;
            }

            LOG_CRITICAL(HW_GPU, "Unhandled texture_info: {}",
                         static_cast<u32>(texture_info.Value()));
            UNREACHABLE();
            return TextureType::Texture1D;
        }

        TextureProcessMode GetTextureProcessMode() const {
            if (texture_info == 1 || texture_info == 5 || texture_info == 12)
                return TextureProcessMode::LL;
            return TextureProcessMode::LZ;
        }

        bool UsesMiscMode(TextureMiscMode mode) const {
            switch (mode) {
            case TextureMiscMode::AOFFI:
                return texture_info == 12 || texture_info == 4;
            case TextureMiscMode::MZ:
                return texture_info == 5;
            case TextureMiscMode::NODEP:
                return nodep_flag != 0;
            default:
                break;
            }
            return false;
        }

        bool IsArrayTexture() const {
            // TEXS only supports Texture2D arrays.
            return texture_info == 8;
        }
    } tlds;

    union {
        BitField<28, 1, u64> is_array;
        BitField<29, 2, TextureType> texture_type;
        BitField<35, 1, u64> aoffi_flag;
        BitField<49, 1, u64> nodep_flag;

        bool UsesMiscMode(TextureMiscMode mode) const {
            switch (mode) {
            case TextureMiscMode::AOFFI:
                return aoffi_flag != 0;
            case TextureMiscMode::NODEP:
                return nodep_flag != 0;
            default:
                break;
            }
            return false;
        }

    } txd;

    union {
        BitField<24, 2, StoreCacheManagement> cache_management;
        BitField<33, 3, ImageType> image_type;
        BitField<49, 2, OutOfBoundsStore> out_of_bounds_store;
        BitField<51, 1, u64> is_immediate;
        BitField<52, 1, SurfaceDataMode> mode;

        BitField<20, 3, StoreType> store_data_layout;
        BitField<20, 4, u64> component_mask_selector;

        bool IsComponentEnabled(std::size_t component) const {
            ASSERT(mode == SurfaceDataMode::P);
            constexpr u8 R = 0b0001;
            constexpr u8 G = 0b0010;
            constexpr u8 B = 0b0100;
            constexpr u8 A = 0b1000;
            constexpr std::array<u8, 16> mask = {
                0,       (R),         (G),         (R | G),        (B),     (R | B),
                (G | B), (R | G | B), (A),         (R | A),        (G | A), (R | G | A),
                (B | A), (R | B | A), (G | B | A), (R | G | B | A)};
            return std::bitset<4>{mask.at(component_mask_selector)}.test(component);
        }

        StoreType GetStoreDataLayout() const {
            ASSERT(mode == SurfaceDataMode::D_BA);
            return store_data_layout;
        }
    } suldst;

    union {
        BitField<28, 1, u64> is_ba;
        BitField<51, 3, ImageAtomicOperationType> operation_type;
        BitField<33, 3, ImageType> image_type;
        BitField<29, 4, ImageAtomicOperation> operation;
        BitField<49, 2, OutOfBoundsStore> out_of_bounds_store;
    } suatom_d;

    union {
        BitField<20, 24, u64> target;
        BitField<5, 1, u64> constant_buffer;

        s32 GetBranchTarget() const {
            // Sign extend the branch target offset
            u32 mask = 1U << (24 - 1);
            u32 value = static_cast<u32>(target);
            // The branch offset is relative to the next instruction and is stored in bytes, so
            // divide it by the size of an instruction and add 1 to it.
            return static_cast<s32>((value ^ mask) - mask) / static_cast<s32>(sizeof(Instruction)) +
                   1;
        }
    } bra;

    union {
        BitField<20, 24, u64> target;
        BitField<5, 1, u64> constant_buffer;

        s32 GetBranchExtend() const {
            // Sign extend the branch target offset
            u32 mask = 1U << (24 - 1);
            u32 value = static_cast<u32>(target);
            // The branch offset is relative to the next instruction and is stored in bytes, so
            // divide it by the size of an instruction and add 1 to it.
            return static_cast<s32>((value ^ mask) - mask) / static_cast<s32>(sizeof(Instruction)) +
                   1;
        }
    } brx;

    union {
        BitField<39, 1, u64> emit; // EmitVertex
        BitField<40, 1, u64> cut;  // EndPrimitive
    } out;

    union {
        BitField<31, 1, u64> skew;
        BitField<32, 1, u64> o;
        BitField<33, 2, IsberdMode> mode;
        BitField<47, 2, IsberdShift> shift;
    } isberd;

    union {
        BitField<8, 2, MembarType> type;
        BitField<0, 2, MembarUnknown> unknown;
    } membar;

    union {
        BitField<48, 1, u64> signed_a;
        BitField<38, 1, u64> is_byte_chunk_a;
        BitField<36, 2, VideoType> type_a;
        BitField<36, 2, u64> byte_height_a;

        BitField<49, 1, u64> signed_b;
        BitField<50, 1, u64> use_register_b;
        BitField<30, 1, u64> is_byte_chunk_b;
        BitField<28, 2, VideoType> type_b;
        BitField<28, 2, u64> byte_height_b;
    } video;

    union {
        BitField<51, 2, VmadShr> shr;
        BitField<55, 1, u64> saturate; // Saturates the result (a * b + c)
        BitField<47, 1, u64> cc;
    } vmad;

    union {
        BitField<54, 1, u64> is_dest_signed;
        BitField<48, 1, u64> is_src_a_signed;
        BitField<49, 1, u64> is_src_b_signed;
        BitField<37, 2, u64> src_format_a;
        BitField<29, 2, u64> src_format_b;
        BitField<56, 1, u64> mx;
        BitField<55, 1, u64> sat;
        BitField<36, 2, u64> selector_a;
        BitField<28, 2, u64> selector_b;
        BitField<50, 1, u64> is_op_b_register;
        BitField<51, 3, VmnmxOperation> operation;

        VmnmxType SourceFormatA() const {
            switch (src_format_a) {
            case 0b11:
                return VmnmxType::Bits32;
            case 0b10:
                return VmnmxType::Bits16;
            default:
                return VmnmxType::Bits8;
            }
        }

        VmnmxType SourceFormatB() const {
            switch (src_format_b) {
            case 0b11:
                return VmnmxType::Bits32;
            case 0b10:
                return VmnmxType::Bits16;
            default:
                return VmnmxType::Bits8;
            }
        }
    } vmnmx;

    union {
        BitField<20, 16, u64> imm20_16;
        BitField<35, 1, u64> high_b_rr; // used on RR
        BitField<36, 1, u64> product_shift_left;
        BitField<37, 1, u64> merge_37;
        BitField<48, 1, u64> sign_a;
        BitField<49, 1, u64> sign_b;
        BitField<50, 2, XmadMode> mode_cbf; // used by CR, RC
        BitField<50, 3, XmadMode> mode;
        BitField<52, 1, u64> high_b;
        BitField<53, 1, u64> high_a;
        BitField<55, 1, u64> product_shift_left_second; // used on CR
        BitField<56, 1, u64> merge_56;
    } xmad;

    union {
        BitField<20, 14, u64> shifted_offset;
        BitField<34, 5, u64> index;

        u64 GetOffset() const {
            return shifted_offset * 4;
        }
    } cbuf34;

    union {
        BitField<20, 16, s64> offset;
        BitField<36, 5, u64> index;

        s64 GetOffset() const {
            return offset;
        }
    } cbuf36;

    // Unsure about the size of this one.
    // It's always used with a gpr0, so any size should be fine.
    BitField<20, 8, SystemVariable> sys20;

    BitField<47, 1, u64> generates_cc;
    BitField<61, 1, u64> is_b_imm;
    BitField<60, 1, u64> is_b_gpr;
    BitField<59, 1, u64> is_c_gpr;
    BitField<20, 24, s64> smem_imm;
    BitField<0, 5, ConditionCode> flow_condition_code;

    Attribute attribute;
    Sampler sampler;
    Image image;

    u64 value;
};
static_assert(sizeof(Instruction) == 0x8, "Incorrect structure size");
static_assert(std::is_standard_layout_v<Instruction>, "Instruction is not standard layout");

class OpCode {
public:
    enum class Id {
        KIL,
        SSY,
        SYNC,
        BRK,
        DEPBAR,
        VOTE,
        VOTE_VTG,
        SHFL,
        FSWZADD,
        BFE_C,
        BFE_R,
        BFE_IMM,
        BFI_RC,
        BFI_IMM_R,
        BRA,
        BRX,
        PBK,
        LD_A,
        LD_L,
        LD_S,
        LD_C,
        LD,  // Load from generic memory
        LDG, // Load from global memory
        ST_A,
        ST_L,
        ST_S,
        ST,    // Store in generic memory
        STG,   // Store in global memory
        RED,   // Reduction operation
        ATOM,  // Atomic operation on global memory
        ATOMS, // Atomic operation on shared memory
        AL2P,  // Transforms attribute memory into physical memory
        TEX,
        TEX_B,  // Texture Load Bindless
        TXQ,    // Texture Query
        TXQ_B,  // Texture Query Bindless
        TEXS,   // Texture Fetch with scalar/non-vec4 source/destinations
        TLD,    // Texture Load
        TLDS,   // Texture Load with scalar/non-vec4 source/destinations
        TLD4,   // Texture Gather 4
        TLD4_B, // Texture Gather 4 Bindless
        TLD4S,  // Texture Load 4 with scalar / non - vec4 source / destinations
        TMML_B, // Texture Mip Map Level
        TMML,   // Texture Mip Map Level
        TXD,    // Texture Gradient/Load with Derivates
        TXD_B,  // Texture Gradient/Load with Derivates Bindless
        SUST,   // Surface Store
        SULD,   // Surface Load
        SUATOM, // Surface Atomic Operation
        EXIT,
        NOP,
        IPA,
        OUT_R, // Emit vertex/primitive
        ISBERD,
        BAR,
        MEMBAR,
        VMAD,
        VSETP,
        VMNMX,
        FFMA_IMM, // Fused Multiply and Add
        FFMA_CR,
        FFMA_RC,
        FFMA_RR,
        FADD_C,
        FADD_R,
        FADD_IMM,
        FADD32I,
        FMUL_C,
        FMUL_R,
        FMUL_IMM,
        FMUL32_IMM,
        IADD_C,
        IADD_R,
        IADD_IMM,
        IADD3_C, // Add 3 Integers
        IADD3_R,
        IADD3_IMM,
        IADD32I,
        ISCADD_C, // Scale and Add
        ISCADD_R,
        ISCADD_IMM,
        FLO_R,
        FLO_C,
        FLO_IMM,
        LEA_R1,
        LEA_R2,
        LEA_RZ,
        LEA_IMM,
        LEA_HI,
        HADD2_C,
        HADD2_R,
        HADD2_IMM,
        HMUL2_C,
        HMUL2_R,
        HMUL2_IMM,
        HFMA2_CR,
        HFMA2_RC,
        HFMA2_RR,
        HFMA2_IMM_R,
        HSETP2_C,
        HSETP2_R,
        HSETP2_IMM,
        HSET2_C,
        HSET2_R,
        HSET2_IMM,
        POPC_C,
        POPC_R,
        POPC_IMM,
        SEL_C,
        SEL_R,
        SEL_IMM,
        ICMP_RC,
        ICMP_R,
        ICMP_CR,
        ICMP_IMM,
        FCMP_RR,
        FCMP_RC,
        FCMP_IMMR,
        MUFU,  // Multi-Function Operator
        RRO_C, // Range Reduction Operator
        RRO_R,
        RRO_IMM,
        F2F_C,
        F2F_R,
        F2F_IMM,
        F2I_C,
        F2I_R,
        F2I_IMM,
        I2F_C,
        I2F_R,
        I2F_IMM,
        I2I_C,
        I2I_R,
        I2I_IMM,
        LOP_C,
        LOP_R,
        LOP_IMM,
        LOP32I,
        LOP3_C,
        LOP3_R,
        LOP3_IMM,
        MOV_C,
        MOV_R,
        MOV_IMM,
        S2R,
        MOV32_IMM,
        SHL_C,
        SHL_R,
        SHL_IMM,
        SHR_C,
        SHR_R,
        SHR_IMM,
        SHF_RIGHT_R,
        SHF_RIGHT_IMM,
        SHF_LEFT_R,
        SHF_LEFT_IMM,
        FMNMX_C,
        FMNMX_R,
        FMNMX_IMM,
        IMNMX_C,
        IMNMX_R,
        IMNMX_IMM,
        FSETP_C, // Set Predicate
        FSETP_R,
        FSETP_IMM,
        FSET_C,
        FSET_R,
        FSET_IMM,
        ISETP_C,
        ISETP_IMM,
        ISETP_R,
        ISET_R,
        ISET_C,
        ISET_IMM,
        PSETP,
        PSET,
        CSETP,
        R2P_IMM,
        P2R_IMM,
        XMAD_IMM,
        XMAD_CR,
        XMAD_RC,
        XMAD_RR,
    };

    enum class Type {
        Trivial,
        Arithmetic,
        ArithmeticImmediate,
        ArithmeticInteger,
        ArithmeticIntegerImmediate,
        ArithmeticHalf,
        ArithmeticHalfImmediate,
        Bfe,
        Bfi,
        Shift,
        Ffma,
        Hfma2,
        Flow,
        Synch,
        Warp,
        Memory,
        Texture,
        Image,
        FloatSet,
        FloatSetPredicate,
        IntegerSet,
        IntegerSetPredicate,
        HalfSet,
        HalfSetPredicate,
        PredicateSetPredicate,
        PredicateSetRegister,
        RegisterSetPredicate,
        Conversion,
        Video,
        Xmad,
        Unknown,
    };

    /// Returns whether an opcode has an execution predicate field or not (ie, whether it can be
    /// conditionally executed).
    static bool IsPredicatedInstruction(Id opcode) {
        // TODO(Subv): Add the rest of unpredicated instructions.
        return opcode != Id::SSY && opcode != Id::PBK;
    }

    class Matcher {
    public:
        constexpr Matcher(const char* const name, u16 mask, u16 expected, Id id, Type type)
            : name{name}, mask{mask}, expected{expected}, id{id}, type{type} {}

        constexpr const char* GetName() const {
            return name;
        }

        constexpr u16 GetMask() const {
            return mask;
        }

        constexpr Id GetId() const {
            return id;
        }

        constexpr Type GetType() const {
            return type;
        }

        /**
         * Tests to see if the given instruction is the instruction this matcher represents.
         * @param instruction The instruction to test
         * @returns true if the given instruction matches.
         */
        constexpr bool Matches(u16 instruction) const {
            return (instruction & mask) == expected;
        }

    private:
        const char* name;
        u16 mask;
        u16 expected;
        Id id;
        Type type;
    };

    static std::optional<std::reference_wrapper<const Matcher>> Decode(Instruction instr) {
        static const auto table{GetDecodeTable()};

        const auto matches_instruction = [instr](const auto& matcher) {
            return matcher.Matches(static_cast<u16>(instr.opcode));
        };

        auto iter = std::find_if(table.begin(), table.end(), matches_instruction);
        return iter != table.end() ? std::optional<std::reference_wrapper<const Matcher>>(*iter)
                                   : std::nullopt;
    }

private:
    struct Detail {
    private:
        static constexpr std::size_t opcode_bitsize = 16;

        /**
         * Generates the mask and the expected value after masking from a given bitstring.
         * A '0' in a bitstring indicates that a zero must be present at that bit position.
         * A '1' in a bitstring indicates that a one must be present at that bit position.
         */
        static constexpr auto GetMaskAndExpect(const char* const bitstring) {
            u16 mask = 0, expect = 0;
            for (std::size_t i = 0; i < opcode_bitsize; i++) {
                const std::size_t bit_position = opcode_bitsize - i - 1;
                switch (bitstring[i]) {
                case '0':
                    mask |= static_cast<u16>(1U << bit_position);
                    break;
                case '1':
                    expect |= static_cast<u16>(1U << bit_position);
                    mask |= static_cast<u16>(1U << bit_position);
                    break;
                default:
                    // Ignore
                    break;
                }
            }
            return std::make_pair(mask, expect);
        }

    public:
        /// Creates a matcher that can match and parse instructions based on bitstring.
        static constexpr auto GetMatcher(const char* const bitstring, Id op, Type type,
                                         const char* const name) {
            const auto [mask, expected] = GetMaskAndExpect(bitstring);
            return Matcher(name, mask, expected, op, type);
        }
    };

    static std::vector<Matcher> GetDecodeTable() {
        std::vector<Matcher> table = {
#define INST(bitstring, op, type, name) Detail::GetMatcher(bitstring, op, type, name)
            INST("111000110011----", Id::KIL, Type::Flow, "KIL"),
            INST("111000101001----", Id::SSY, Type::Flow, "SSY"),
            INST("111000101010----", Id::PBK, Type::Flow, "PBK"),
            INST("111000100100----", Id::BRA, Type::Flow, "BRA"),
            INST("111000100101----", Id::BRX, Type::Flow, "BRX"),
            INST("1111000011111---", Id::SYNC, Type::Flow, "SYNC"),
            INST("111000110100----", Id::BRK, Type::Flow, "BRK"),
            INST("111000110000----", Id::EXIT, Type::Flow, "EXIT"),
            INST("1111000011110---", Id::DEPBAR, Type::Synch, "DEPBAR"),
            INST("0101000011011---", Id::VOTE, Type::Warp, "VOTE"),
            INST("0101000011100---", Id::VOTE_VTG, Type::Warp, "VOTE_VTG"),
            INST("1110111100010---", Id::SHFL, Type::Warp, "SHFL"),
            INST("0101000011111---", Id::FSWZADD, Type::Warp, "FSWZADD"),
            INST("1110111111011---", Id::LD_A, Type::Memory, "LD_A"),
            INST("1110111101001---", Id::LD_S, Type::Memory, "LD_S"),
            INST("1110111101000---", Id::LD_L, Type::Memory, "LD_L"),
            INST("1110111110010---", Id::LD_C, Type::Memory, "LD_C"),
            INST("100-------------", Id::LD, Type::Memory, "LD"),
            INST("1110111011010---", Id::LDG, Type::Memory, "LDG"),
            INST("1110111111110---", Id::ST_A, Type::Memory, "ST_A"),
            INST("1110111101011---", Id::ST_S, Type::Memory, "ST_S"),
            INST("1110111101010---", Id::ST_L, Type::Memory, "ST_L"),
            INST("101-------------", Id::ST, Type::Memory, "ST"),
            INST("1110111011011---", Id::STG, Type::Memory, "STG"),
            INST("1110101111111---", Id::RED, Type::Memory, "RED"),
            INST("11101101--------", Id::ATOM, Type::Memory, "ATOM"),
            INST("11101100--------", Id::ATOMS, Type::Memory, "ATOMS"),
            INST("1110111110100---", Id::AL2P, Type::Memory, "AL2P"),
            INST("110000----111---", Id::TEX, Type::Texture, "TEX"),
            INST("1101111010111---", Id::TEX_B, Type::Texture, "TEX_B"),
            INST("1101111101001---", Id::TXQ, Type::Texture, "TXQ"),
            INST("1101111101010---", Id::TXQ_B, Type::Texture, "TXQ_B"),
            INST("1101-00---------", Id::TEXS, Type::Texture, "TEXS"),
            INST("11011100--11----", Id::TLD, Type::Texture, "TLD"),
            INST("1101-01---------", Id::TLDS, Type::Texture, "TLDS"),
            INST("110010----111---", Id::TLD4, Type::Texture, "TLD4"),
            INST("1101111011111---", Id::TLD4_B, Type::Texture, "TLD4_B"),
            INST("11011111-0------", Id::TLD4S, Type::Texture, "TLD4S"),
            INST("110111110110----", Id::TMML_B, Type::Texture, "TMML_B"),
            INST("1101111101011---", Id::TMML, Type::Texture, "TMML"),
            INST("11011110011110--", Id::TXD_B, Type::Texture, "TXD_B"),
            INST("11011110001110--", Id::TXD, Type::Texture, "TXD"),
            INST("11101011001-----", Id::SUST, Type::Image, "SUST"),
            INST("11101011000-----", Id::SULD, Type::Image, "SULD"),
            INST("1110101000------", Id::SUATOM, Type::Image, "SUATOM_D"),
            INST("0101000010110---", Id::NOP, Type::Trivial, "NOP"),
            INST("11100000--------", Id::IPA, Type::Trivial, "IPA"),
            INST("1111101111100---", Id::OUT_R, Type::Trivial, "OUT_R"),
            INST("1110111111010---", Id::ISBERD, Type::Trivial, "ISBERD"),
            INST("1111000010101---", Id::BAR, Type::Trivial, "BAR"),
            INST("1110111110011---", Id::MEMBAR, Type::Trivial, "MEMBAR"),
            INST("01011111--------", Id::VMAD, Type::Video, "VMAD"),
            INST("0101000011110---", Id::VSETP, Type::Video, "VSETP"),
            INST("0011101---------", Id::VMNMX, Type::Video, "VMNMX"),
            INST("0011001-1-------", Id::FFMA_IMM, Type::Ffma, "FFMA_IMM"),
            INST("010010011-------", Id::FFMA_CR, Type::Ffma, "FFMA_CR"),
            INST("010100011-------", Id::FFMA_RC, Type::Ffma, "FFMA_RC"),
            INST("010110011-------", Id::FFMA_RR, Type::Ffma, "FFMA_RR"),
            INST("0100110001011---", Id::FADD_C, Type::Arithmetic, "FADD_C"),
            INST("0101110001011---", Id::FADD_R, Type::Arithmetic, "FADD_R"),
            INST("0011100-01011---", Id::FADD_IMM, Type::Arithmetic, "FADD_IMM"),
            INST("000010----------", Id::FADD32I, Type::ArithmeticImmediate, "FADD32I"),
            INST("0100110001101---", Id::FMUL_C, Type::Arithmetic, "FMUL_C"),
            INST("0101110001101---", Id::FMUL_R, Type::Arithmetic, "FMUL_R"),
            INST("0011100-01101---", Id::FMUL_IMM, Type::Arithmetic, "FMUL_IMM"),
            INST("00011110--------", Id::FMUL32_IMM, Type::ArithmeticImmediate, "FMUL32_IMM"),
            INST("0100110000010---", Id::IADD_C, Type::ArithmeticInteger, "IADD_C"),
            INST("0101110000010---", Id::IADD_R, Type::ArithmeticInteger, "IADD_R"),
            INST("0011100-00010---", Id::IADD_IMM, Type::ArithmeticInteger, "IADD_IMM"),
            INST("010011001100----", Id::IADD3_C, Type::ArithmeticInteger, "IADD3_C"),
            INST("010111001100----", Id::IADD3_R, Type::ArithmeticInteger, "IADD3_R"),
            INST("0011100-1100----", Id::IADD3_IMM, Type::ArithmeticInteger, "IADD3_IMM"),
            INST("0001110---------", Id::IADD32I, Type::ArithmeticIntegerImmediate, "IADD32I"),
            INST("0100110000011---", Id::ISCADD_C, Type::ArithmeticInteger, "ISCADD_C"),
            INST("0101110000011---", Id::ISCADD_R, Type::ArithmeticInteger, "ISCADD_R"),
            INST("0011100-00011---", Id::ISCADD_IMM, Type::ArithmeticInteger, "ISCADD_IMM"),
            INST("0100110000001---", Id::POPC_C, Type::ArithmeticInteger, "POPC_C"),
            INST("0101110000001---", Id::POPC_R, Type::ArithmeticInteger, "POPC_R"),
            INST("0011100-00001---", Id::POPC_IMM, Type::ArithmeticInteger, "POPC_IMM"),
            INST("0100110010100---", Id::SEL_C, Type::ArithmeticInteger, "SEL_C"),
            INST("0101110010100---", Id::SEL_R, Type::ArithmeticInteger, "SEL_R"),
            INST("0011100-10100---", Id::SEL_IMM, Type::ArithmeticInteger, "SEL_IMM"),
            INST("010100110100----", Id::ICMP_RC, Type::ArithmeticInteger, "ICMP_RC"),
            INST("010110110100----", Id::ICMP_R, Type::ArithmeticInteger, "ICMP_R"),
            INST("010010110100----", Id::ICMP_CR, Type::ArithmeticInteger, "ICMP_CR"),
            INST("0011011-0100----", Id::ICMP_IMM, Type::ArithmeticInteger, "ICMP_IMM"),
            INST("0101110000110---", Id::FLO_R, Type::ArithmeticInteger, "FLO_R"),
            INST("0100110000110---", Id::FLO_C, Type::ArithmeticInteger, "FLO_C"),
            INST("0011100-00110---", Id::FLO_IMM, Type::ArithmeticInteger, "FLO_IMM"),
            INST("0101101111011---", Id::LEA_R2, Type::ArithmeticInteger, "LEA_R2"),
            INST("0101101111010---", Id::LEA_R1, Type::ArithmeticInteger, "LEA_R1"),
            INST("001101101101----", Id::LEA_IMM, Type::ArithmeticInteger, "LEA_IMM"),
            INST("010010111101----", Id::LEA_RZ, Type::ArithmeticInteger, "LEA_RZ"),
            INST("00011000--------", Id::LEA_HI, Type::ArithmeticInteger, "LEA_HI"),
            INST("0111101-1-------", Id::HADD2_C, Type::ArithmeticHalf, "HADD2_C"),
            INST("0101110100010---", Id::HADD2_R, Type::ArithmeticHalf, "HADD2_R"),
            INST("0111101-0-------", Id::HADD2_IMM, Type::ArithmeticHalfImmediate, "HADD2_IMM"),
            INST("0111100-1-------", Id::HMUL2_C, Type::ArithmeticHalf, "HMUL2_C"),
            INST("0101110100001---", Id::HMUL2_R, Type::ArithmeticHalf, "HMUL2_R"),
            INST("0111100-0-------", Id::HMUL2_IMM, Type::ArithmeticHalfImmediate, "HMUL2_IMM"),
            INST("01110---1-------", Id::HFMA2_CR, Type::Hfma2, "HFMA2_CR"),
            INST("01100---1-------", Id::HFMA2_RC, Type::Hfma2, "HFMA2_RC"),
            INST("0101110100000---", Id::HFMA2_RR, Type::Hfma2, "HFMA2_RR"),
            INST("01110---0-------", Id::HFMA2_IMM_R, Type::Hfma2, "HFMA2_R_IMM"),
            INST("0111111-1-------", Id::HSETP2_C, Type::HalfSetPredicate, "HSETP2_C"),
            INST("0101110100100---", Id::HSETP2_R, Type::HalfSetPredicate, "HSETP2_R"),
            INST("0111111-0-------", Id::HSETP2_IMM, Type::HalfSetPredicate, "HSETP2_IMM"),
            INST("0111110-1-------", Id::HSET2_C, Type::HalfSet, "HSET2_C"),
            INST("0101110100011---", Id::HSET2_R, Type::HalfSet, "HSET2_R"),
            INST("0111110-0-------", Id::HSET2_IMM, Type::HalfSet, "HSET2_IMM"),
            INST("010110111010----", Id::FCMP_RR, Type::Arithmetic, "FCMP_RR"),
            INST("010010111010----", Id::FCMP_RC, Type::Arithmetic, "FCMP_RC"),
            INST("0011011-1010----", Id::FCMP_IMMR, Type::Arithmetic, "FCMP_IMMR"),
            INST("0101000010000---", Id::MUFU, Type::Arithmetic, "MUFU"),
            INST("0100110010010---", Id::RRO_C, Type::Arithmetic, "RRO_C"),
            INST("0101110010010---", Id::RRO_R, Type::Arithmetic, "RRO_R"),
            INST("0011100-10010---", Id::RRO_IMM, Type::Arithmetic, "RRO_IMM"),
            INST("0100110010101---", Id::F2F_C, Type::Conversion, "F2F_C"),
            INST("0101110010101---", Id::F2F_R, Type::Conversion, "F2F_R"),
            INST("0011100-10101---", Id::F2F_IMM, Type::Conversion, "F2F_IMM"),
            INST("0100110010110---", Id::F2I_C, Type::Conversion, "F2I_C"),
            INST("0101110010110---", Id::F2I_R, Type::Conversion, "F2I_R"),
            INST("0011100-10110---", Id::F2I_IMM, Type::Conversion, "F2I_IMM"),
            INST("0100110010011---", Id::MOV_C, Type::Arithmetic, "MOV_C"),
            INST("0101110010011---", Id::MOV_R, Type::Arithmetic, "MOV_R"),
            INST("0011100-10011---", Id::MOV_IMM, Type::Arithmetic, "MOV_IMM"),
            INST("1111000011001---", Id::S2R, Type::Trivial, "S2R"),
            INST("000000010000----", Id::MOV32_IMM, Type::ArithmeticImmediate, "MOV32_IMM"),
            INST("0100110001100---", Id::FMNMX_C, Type::Arithmetic, "FMNMX_C"),
            INST("0101110001100---", Id::FMNMX_R, Type::Arithmetic, "FMNMX_R"),
            INST("0011100-01100---", Id::FMNMX_IMM, Type::Arithmetic, "FMNMX_IMM"),
            INST("0100110000100---", Id::IMNMX_C, Type::ArithmeticInteger, "IMNMX_C"),
            INST("0101110000100---", Id::IMNMX_R, Type::ArithmeticInteger, "IMNMX_R"),
            INST("0011100-00100---", Id::IMNMX_IMM, Type::ArithmeticInteger, "IMNMX_IMM"),
            INST("0100110000000---", Id::BFE_C, Type::Bfe, "BFE_C"),
            INST("0101110000000---", Id::BFE_R, Type::Bfe, "BFE_R"),
            INST("0011100-00000---", Id::BFE_IMM, Type::Bfe, "BFE_IMM"),
            INST("0101001111110---", Id::BFI_RC, Type::Bfi, "BFI_RC"),
            INST("0011011-11110---", Id::BFI_IMM_R, Type::Bfi, "BFI_IMM_R"),
            INST("0100110001000---", Id::LOP_C, Type::ArithmeticInteger, "LOP_C"),
            INST("0101110001000---", Id::LOP_R, Type::ArithmeticInteger, "LOP_R"),
            INST("0011100-01000---", Id::LOP_IMM, Type::ArithmeticInteger, "LOP_IMM"),
            INST("000001----------", Id::LOP32I, Type::ArithmeticIntegerImmediate, "LOP32I"),
            INST("0000001---------", Id::LOP3_C, Type::ArithmeticInteger, "LOP3_C"),
            INST("0101101111100---", Id::LOP3_R, Type::ArithmeticInteger, "LOP3_R"),
            INST("0011110---------", Id::LOP3_IMM, Type::ArithmeticInteger, "LOP3_IMM"),
            INST("0100110001001---", Id::SHL_C, Type::Shift, "SHL_C"),
            INST("0101110001001---", Id::SHL_R, Type::Shift, "SHL_R"),
            INST("0011100-01001---", Id::SHL_IMM, Type::Shift, "SHL_IMM"),
            INST("0100110000101---", Id::SHR_C, Type::Shift, "SHR_C"),
            INST("0101110000101---", Id::SHR_R, Type::Shift, "SHR_R"),
            INST("0011100-00101---", Id::SHR_IMM, Type::Shift, "SHR_IMM"),
            INST("0101110011111---", Id::SHF_RIGHT_R, Type::Shift, "SHF_RIGHT_R"),
            INST("0011100-11111---", Id::SHF_RIGHT_IMM, Type::Shift, "SHF_RIGHT_IMM"),
            INST("0101101111111---", Id::SHF_LEFT_R, Type::Shift, "SHF_LEFT_R"),
            INST("0011011-11111---", Id::SHF_LEFT_IMM, Type::Shift, "SHF_LEFT_IMM"),
            INST("0100110011100---", Id::I2I_C, Type::Conversion, "I2I_C"),
            INST("0101110011100---", Id::I2I_R, Type::Conversion, "I2I_R"),
            INST("0011100-11100---", Id::I2I_IMM, Type::Conversion, "I2I_IMM"),
            INST("0100110010111---", Id::I2F_C, Type::Conversion, "I2F_C"),
            INST("0101110010111---", Id::I2F_R, Type::Conversion, "I2F_R"),
            INST("0011100-10111---", Id::I2F_IMM, Type::Conversion, "I2F_IMM"),
            INST("01011000--------", Id::FSET_R, Type::FloatSet, "FSET_R"),
            INST("0100100---------", Id::FSET_C, Type::FloatSet, "FSET_C"),
            INST("0011000---------", Id::FSET_IMM, Type::FloatSet, "FSET_IMM"),
            INST("010010111011----", Id::FSETP_C, Type::FloatSetPredicate, "FSETP_C"),
            INST("010110111011----", Id::FSETP_R, Type::FloatSetPredicate, "FSETP_R"),
            INST("0011011-1011----", Id::FSETP_IMM, Type::FloatSetPredicate, "FSETP_IMM"),
            INST("010010110110----", Id::ISETP_C, Type::IntegerSetPredicate, "ISETP_C"),
            INST("010110110110----", Id::ISETP_R, Type::IntegerSetPredicate, "ISETP_R"),
            INST("0011011-0110----", Id::ISETP_IMM, Type::IntegerSetPredicate, "ISETP_IMM"),
            INST("010110110101----", Id::ISET_R, Type::IntegerSet, "ISET_R"),
            INST("010010110101----", Id::ISET_C, Type::IntegerSet, "ISET_C"),
            INST("0011011-0101----", Id::ISET_IMM, Type::IntegerSet, "ISET_IMM"),
            INST("0101000010001---", Id::PSET, Type::PredicateSetRegister, "PSET"),
            INST("0101000010010---", Id::PSETP, Type::PredicateSetPredicate, "PSETP"),
            INST("010100001010----", Id::CSETP, Type::PredicateSetPredicate, "CSETP"),
            INST("0011100-11110---", Id::R2P_IMM, Type::RegisterSetPredicate, "R2P_IMM"),
            INST("0011100-11101---", Id::P2R_IMM, Type::RegisterSetPredicate, "P2R_IMM"),
            INST("0011011-00------", Id::XMAD_IMM, Type::Xmad, "XMAD_IMM"),
            INST("0100111---------", Id::XMAD_CR, Type::Xmad, "XMAD_CR"),
            INST("010100010-------", Id::XMAD_RC, Type::Xmad, "XMAD_RC"),
            INST("0101101100------", Id::XMAD_RR, Type::Xmad, "XMAD_RR"),
        };
#undef INST
        std::stable_sort(table.begin(), table.end(), [](const auto& a, const auto& b) {
            // If a matcher has more bits in its mask it is more specific, so it
            // should come first.
            return std::bitset<16>(a.GetMask()).count() > std::bitset<16>(b.GetMask()).count();
        });

        return table;
    }
};

} // namespace Tegra::Shader
