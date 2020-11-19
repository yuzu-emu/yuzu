// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "common/common_types.h"
#include "video_core/engines/shader_bytecode.h"

namespace VideoCommon::Shader {

enum class OperationCode {
    Assign, /// (float& dest, float src) -> void

    Select, /// (MetaArithmetic, bool pred, float a, float b) -> float

    FAdd,          /// (MetaArithmetic, float a, float b) -> float
    FMul,          /// (MetaArithmetic, float a, float b) -> float
    FDiv,          /// (MetaArithmetic, float a, float b) -> float
    FFma,          /// (MetaArithmetic, float a, float b, float c) -> float
    FNegate,       /// (MetaArithmetic, float a) -> float
    FAbsolute,     /// (MetaArithmetic, float a) -> float
    FClamp,        /// (MetaArithmetic, float value, float min, float max) -> float
    FCastHalf0,    /// (MetaArithmetic, f16vec2 a) -> float
    FCastHalf1,    /// (MetaArithmetic, f16vec2 a) -> float
    FMin,          /// (MetaArithmetic, float a, float b) -> float
    FMax,          /// (MetaArithmetic, float a, float b) -> float
    FCos,          /// (MetaArithmetic, float a) -> float
    FSin,          /// (MetaArithmetic, float a) -> float
    FExp2,         /// (MetaArithmetic, float a) -> float
    FLog2,         /// (MetaArithmetic, float a) -> float
    FInverseSqrt,  /// (MetaArithmetic, float a) -> float
    FSqrt,         /// (MetaArithmetic, float a) -> float
    FRoundEven,    /// (MetaArithmetic, float a) -> float
    FFloor,        /// (MetaArithmetic, float a) -> float
    FCeil,         /// (MetaArithmetic, float a) -> float
    FTrunc,        /// (MetaArithmetic, float a) -> float
    FCastInteger,  /// (MetaArithmetic, int a) -> float
    FCastUInteger, /// (MetaArithmetic, uint a) -> float
    FSwizzleAdd,   /// (float a, float b, uint mask) -> float

    IAdd,                  /// (MetaArithmetic, int a, int b) -> int
    IMul,                  /// (MetaArithmetic, int a, int b) -> int
    IDiv,                  /// (MetaArithmetic, int a, int b) -> int
    INegate,               /// (MetaArithmetic, int a) -> int
    IAbsolute,             /// (MetaArithmetic, int a) -> int
    IMin,                  /// (MetaArithmetic, int a, int b) -> int
    IMax,                  /// (MetaArithmetic, int a, int b) -> int
    ICastFloat,            /// (MetaArithmetic, float a) -> int
    ICastUnsigned,         /// (MetaArithmetic, uint a) -> int
    ILogicalShiftLeft,     /// (MetaArithmetic, int a, uint b) -> int
    ILogicalShiftRight,    /// (MetaArithmetic, int a, uint b) -> int
    IArithmeticShiftRight, /// (MetaArithmetic, int a, uint b) -> int
    IBitwiseAnd,           /// (MetaArithmetic, int a, int b) -> int
    IBitwiseOr,            /// (MetaArithmetic, int a, int b) -> int
    IBitwiseXor,           /// (MetaArithmetic, int a, int b) -> int
    IBitwiseNot,           /// (MetaArithmetic, int a) -> int
    IBitfieldInsert,       /// (MetaArithmetic, int base, int insert, int offset, int bits) -> int
    IBitfieldExtract,      /// (MetaArithmetic, int value, int offset, int offset) -> int
    IBitCount,             /// (MetaArithmetic, int) -> int
    IBitMSB,               /// (MetaArithmetic, int) -> int

    UAdd,                  /// (MetaArithmetic, uint a, uint b) -> uint
    UMul,                  /// (MetaArithmetic, uint a, uint b) -> uint
    UDiv,                  /// (MetaArithmetic, uint a, uint b) -> uint
    UMin,                  /// (MetaArithmetic, uint a, uint b) -> uint
    UMax,                  /// (MetaArithmetic, uint a, uint b) -> uint
    UCastFloat,            /// (MetaArithmetic, float a) -> uint
    UCastSigned,           /// (MetaArithmetic, int a) -> uint
    ULogicalShiftLeft,     /// (MetaArithmetic, uint a, uint b) -> uint
    ULogicalShiftRight,    /// (MetaArithmetic, uint a, uint b) -> uint
    UArithmeticShiftRight, /// (MetaArithmetic, uint a, uint b) -> uint
    UBitwiseAnd,           /// (MetaArithmetic, uint a, uint b) -> uint
    UBitwiseOr,            /// (MetaArithmetic, uint a, uint b) -> uint
    UBitwiseXor,           /// (MetaArithmetic, uint a, uint b) -> uint
    UBitwiseNot,           /// (MetaArithmetic, uint a) -> uint
    UBitfieldInsert,  /// (MetaArithmetic, uint base, uint insert, int offset, int bits) -> uint
    UBitfieldExtract, /// (MetaArithmetic, uint value, int offset, int offset) -> uint
    UBitCount,        /// (MetaArithmetic, uint) -> uint
    UBitMSB,          /// (MetaArithmetic, uint) -> uint

    HAdd,       /// (MetaArithmetic, f16vec2 a, f16vec2 b) -> f16vec2
    HMul,       /// (MetaArithmetic, f16vec2 a, f16vec2 b) -> f16vec2
    HFma,       /// (MetaArithmetic, f16vec2 a, f16vec2 b, f16vec2 c) -> f16vec2
    HAbsolute,  /// (f16vec2 a) -> f16vec2
    HNegate,    /// (f16vec2 a, bool first, bool second) -> f16vec2
    HClamp,     /// (f16vec2 src, float min, float max) -> f16vec2
    HCastFloat, /// (MetaArithmetic, float a) -> f16vec2
    HUnpack,    /// (Tegra::Shader::HalfType, T value) -> f16vec2
    HMergeF32,  /// (f16vec2 src) -> float
    HMergeH0,   /// (f16vec2 dest, f16vec2 src) -> f16vec2
    HMergeH1,   /// (f16vec2 dest, f16vec2 src) -> f16vec2
    HPack2,     /// (float a, float b) -> f16vec2

    LogicalAssign, /// (bool& dst, bool src) -> void
    LogicalAnd,    /// (bool a, bool b) -> bool
    LogicalOr,     /// (bool a, bool b) -> bool
    LogicalXor,    /// (bool a, bool b) -> bool
    LogicalNegate, /// (bool a) -> bool
    LogicalPick2,  /// (bool2 pair, uint index) -> bool
    LogicalAnd2,   /// (bool2 a) -> bool

    LogicalFOrdLessThan,       /// (float a, float b) -> bool
    LogicalFOrdEqual,          /// (float a, float b) -> bool
    LogicalFOrdLessEqual,      /// (float a, float b) -> bool
    LogicalFOrdGreaterThan,    /// (float a, float b) -> bool
    LogicalFOrdNotEqual,       /// (float a, float b) -> bool
    LogicalFOrdGreaterEqual,   /// (float a, float b) -> bool
    LogicalFOrdered,           /// (float a, float b) -> bool
    LogicalFUnordered,         /// (float a, float b) -> bool
    LogicalFUnordLessThan,     /// (float a, float b) -> bool
    LogicalFUnordEqual,        /// (float a, float b) -> bool
    LogicalFUnordLessEqual,    /// (float a, float b) -> bool
    LogicalFUnordGreaterThan,  /// (float a, float b) -> bool
    LogicalFUnordNotEqual,     /// (float a, float b) -> bool
    LogicalFUnordGreaterEqual, /// (float a, float b) -> bool

    LogicalILessThan,     /// (int a, int b) -> bool
    LogicalIEqual,        /// (int a, int b) -> bool
    LogicalILessEqual,    /// (int a, int b) -> bool
    LogicalIGreaterThan,  /// (int a, int b) -> bool
    LogicalINotEqual,     /// (int a, int b) -> bool
    LogicalIGreaterEqual, /// (int a, int b) -> bool

    LogicalULessThan,     /// (uint a, uint b) -> bool
    LogicalUEqual,        /// (uint a, uint b) -> bool
    LogicalULessEqual,    /// (uint a, uint b) -> bool
    LogicalUGreaterThan,  /// (uint a, uint b) -> bool
    LogicalUNotEqual,     /// (uint a, uint b) -> bool
    LogicalUGreaterEqual, /// (uint a, uint b) -> bool

    LogicalAddCarry, /// (uint a, uint b) -> bool

    Logical2HLessThan,            /// (MetaHalfArithmetic, f16vec2 a, f16vec2) -> bool2
    Logical2HEqual,               /// (MetaHalfArithmetic, f16vec2 a, f16vec2) -> bool2
    Logical2HLessEqual,           /// (MetaHalfArithmetic, f16vec2 a, f16vec2) -> bool2
    Logical2HGreaterThan,         /// (MetaHalfArithmetic, f16vec2 a, f16vec2) -> bool2
    Logical2HNotEqual,            /// (MetaHalfArithmetic, f16vec2 a, f16vec2) -> bool2
    Logical2HGreaterEqual,        /// (MetaHalfArithmetic, f16vec2 a, f16vec2) -> bool2
    Logical2HLessThanWithNan,     /// (MetaHalfArithmetic, f16vec2 a, f16vec2) -> bool2
    Logical2HEqualWithNan,        /// (MetaHalfArithmetic, f16vec2 a, f16vec2) -> bool2
    Logical2HLessEqualWithNan,    /// (MetaHalfArithmetic, f16vec2 a, f16vec2) -> bool2
    Logical2HGreaterThanWithNan,  /// (MetaHalfArithmetic, f16vec2 a, f16vec2) -> bool2
    Logical2HNotEqualWithNan,     /// (MetaHalfArithmetic, f16vec2 a, f16vec2) -> bool2
    Logical2HGreaterEqualWithNan, /// (MetaHalfArithmetic, f16vec2 a, f16vec2) -> bool2

    Texture,                /// (MetaTexture, float[N] coords) -> float4
    TextureLod,             /// (MetaTexture, float[N] coords) -> float4
    TextureGather,          /// (MetaTexture, float[N] coords) -> float4
    TextureQueryDimensions, /// (MetaTexture, float a) -> float4
    TextureQueryLod,        /// (MetaTexture, float[N] coords) -> float4
    TexelFetch,             /// (MetaTexture, int[N], int) -> float4
    TextureGradient,        /// (MetaTexture, float[N] coords, float[N*2] derivates) -> float4

    ImageLoad,  /// (MetaImage, int[N] coords) -> void
    ImageStore, /// (MetaImage, int[N] coords) -> void

    AtomicImageAdd,      /// (MetaImage, int[N] coords) -> void
    AtomicImageAnd,      /// (MetaImage, int[N] coords) -> void
    AtomicImageOr,       /// (MetaImage, int[N] coords) -> void
    AtomicImageXor,      /// (MetaImage, int[N] coords) -> void
    AtomicImageExchange, /// (MetaImage, int[N] coords) -> void

    AtomicUExchange, /// (memory, uint) -> uint
    AtomicUAdd,      /// (memory, uint) -> uint
    AtomicUMin,      /// (memory, uint) -> uint
    AtomicUMax,      /// (memory, uint) -> uint
    AtomicUAnd,      /// (memory, uint) -> uint
    AtomicUOr,       /// (memory, uint) -> uint
    AtomicUXor,      /// (memory, uint) -> uint

    AtomicIExchange, /// (memory, int) -> int
    AtomicIAdd,      /// (memory, int) -> int
    AtomicIMin,      /// (memory, int) -> int
    AtomicIMax,      /// (memory, int) -> int
    AtomicIAnd,      /// (memory, int) -> int
    AtomicIOr,       /// (memory, int) -> int
    AtomicIXor,      /// (memory, int) -> int

    ReduceUAdd, /// (memory, uint) -> void
    ReduceUMin, /// (memory, uint) -> void
    ReduceUMax, /// (memory, uint) -> void
    ReduceUAnd, /// (memory, uint) -> void
    ReduceUOr,  /// (memory, uint) -> void
    ReduceUXor, /// (memory, uint) -> void

    ReduceIAdd, /// (memory, int) -> void
    ReduceIMin, /// (memory, int) -> void
    ReduceIMax, /// (memory, int) -> void
    ReduceIAnd, /// (memory, int) -> void
    ReduceIOr,  /// (memory, int) -> void
    ReduceIXor, /// (memory, int) -> void

    Branch,         /// (uint branch_target) -> void
    BranchIndirect, /// (uint branch_target) -> void
    PushFlowStack,  /// (uint branch_target) -> void
    PopFlowStack,   /// () -> void
    Exit,           /// () -> void
    Discard,        /// () -> void

    EmitVertex,   /// () -> void
    EndPrimitive, /// () -> void

    InvocationId,       /// () -> int
    YNegate,            /// () -> float
    LocalInvocationIdX, /// () -> uint
    LocalInvocationIdY, /// () -> uint
    LocalInvocationIdZ, /// () -> uint
    WorkGroupIdX,       /// () -> uint
    WorkGroupIdY,       /// () -> uint
    WorkGroupIdZ,       /// () -> uint

    BallotThread, /// (bool) -> uint
    VoteAll,      /// (bool) -> bool
    VoteAny,      /// (bool) -> bool
    VoteEqual,    /// (bool) -> bool

    ThreadId,       /// () -> uint
    ThreadEqMask,   /// () -> uint
    ThreadGeMask,   /// () -> uint
    ThreadGtMask,   /// () -> uint
    ThreadLeMask,   /// () -> uint
    ThreadLtMask,   /// () -> uint
    ShuffleIndexed, /// (uint value, uint index) -> uint

    Barrier,             /// () -> void
    MemoryBarrierGroup,  /// () -> void
    MemoryBarrierGlobal, /// () -> void

    Amount,
};

enum class InternalFlag {
    Zero = 0,
    Sign = 1,
    Carry = 2,
    Overflow = 3,
    Amount = 4,
};

enum class MetaStackClass {
    Ssy,
    Pbk,
};

class AbufNode;
class CbufNode;
class CommentNode;
class ConditionalNode;
class CustomVarNode;
class GmemNode;
class GprNode;
class ImmediateNode;
class InternalFlagNode;
class LmemNode;
class OperationNode;
class PatchNode;
class PredicateNode;
class SmemNode;

using NodeData = std::variant<OperationNode, ConditionalNode, GprNode, CustomVarNode, ImmediateNode,
                              InternalFlagNode, PredicateNode, AbufNode, PatchNode, CbufNode,
                              LmemNode, SmemNode, GmemNode, CommentNode>;
using Node = std::shared_ptr<NodeData>;
using Node4 = std::array<Node, 4>;
using NodeBlock = std::vector<Node>;

struct ArraySamplerNode;
struct BindlessSamplerNode;
struct SeparateSamplerNode;

using TrackSamplerData = std::variant<BindlessSamplerNode, SeparateSamplerNode, ArraySamplerNode>;
using TrackSampler = std::shared_ptr<TrackSamplerData>;

struct Sampler {
    /// Bound samplers constructor
    constexpr explicit Sampler(u32 index, u32 offset, Tegra::Shader::TextureType type,
                               bool is_array, bool is_shadow, bool is_buffer, bool is_indexed)
        : index{index}, offset{offset}, type{type}, is_array{is_array}, is_shadow{is_shadow},
          is_buffer{is_buffer}, is_indexed{is_indexed} {}

    /// Separate sampler constructor
    constexpr explicit Sampler(u32 index, std::pair<u32, u32> offsets, std::pair<u32, u32> buffers,
                               Tegra::Shader::TextureType type, bool is_array, bool is_shadow,
                               bool is_buffer)
        : index{index}, offset{offsets.first}, secondary_offset{offsets.second},
          buffer{buffers.first}, secondary_buffer{buffers.second}, type{type}, is_array{is_array},
          is_shadow{is_shadow}, is_buffer{is_buffer}, is_separated{true} {}

    /// Bindless samplers constructor
    constexpr explicit Sampler(u32 index, u32 offset, u32 buffer, Tegra::Shader::TextureType type,
                               bool is_array, bool is_shadow, bool is_buffer, bool is_indexed)
        : index{index}, offset{offset}, buffer{buffer}, type{type}, is_array{is_array},
          is_shadow{is_shadow}, is_buffer{is_buffer}, is_bindless{true}, is_indexed{is_indexed} {}

    u32 index = 0;            ///< Emulated index given for the this sampler.
    u32 offset = 0;           ///< Offset in the const buffer from where the sampler is being read.
    u32 secondary_offset = 0; ///< Secondary offset in the const buffer.
    u32 buffer = 0;           ///< Buffer where the bindless sampler is read.
    u32 secondary_buffer = 0; ///< Secondary buffer where the bindless sampler is read.
    u32 size = 1;             ///< Size of the sampler.

    Tegra::Shader::TextureType type{}; ///< The type used to sample this texture (Texture2D, etc)
    bool is_array = false;     ///< Whether the texture is being sampled as an array texture or not.
    bool is_shadow = false;    ///< Whether the texture is being sampled as a depth texture or not.
    bool is_buffer = false;    ///< Whether the texture is a texture buffer without sampler.
    bool is_bindless = false;  ///< Whether this sampler belongs to a bindless texture or not.
    bool is_indexed = false;   ///< Whether this sampler is an indexed array of textures.
    bool is_separated = false; ///< Whether the image and sampler is separated or not.
};

/// Represents a tracked bindless sampler into a direct const buffer
struct ArraySamplerNode {
    u32 index;
    u32 base_offset;
    u32 bindless_var;
};

/// Represents a tracked separate sampler image pair that was folded statically
struct SeparateSamplerNode {
    std::pair<u32, u32> indices;
    std::pair<u32, u32> offsets;
};

/// Represents a tracked bindless sampler into a direct const buffer
struct BindlessSamplerNode {
    u32 index;
    u32 offset;
};

struct Image {
public:
    /// Bound images constructor
    constexpr explicit Image(u32 index, u32 offset, Tegra::Shader::ImageType type)
        : index{index}, offset{offset}, type{type} {}

    /// Bindless samplers constructor
    constexpr explicit Image(u32 index, u32 offset, u32 buffer, Tegra::Shader::ImageType type)
        : index{index}, offset{offset}, buffer{buffer}, type{type}, is_bindless{true} {}

    void MarkWrite() {
        is_written = true;
    }

    void MarkRead() {
        is_read = true;
    }

    void MarkAtomic() {
        MarkWrite();
        MarkRead();
        is_atomic = true;
    }

    u32 index = 0;
    u32 offset = 0;
    u32 buffer = 0;

    Tegra::Shader::ImageType type{};
    bool is_bindless = false;
    bool is_written = false;
    bool is_read = false;
    bool is_atomic = false;
};

struct GlobalMemoryBase {
    u32 cbuf_index = 0;
    u32 cbuf_offset = 0;

    auto operator<=>(const GlobalMemoryBase& rhs) const = default;
};

/// Parameters describing an arithmetic operation
struct MetaArithmetic {
    bool precise{}; ///< Whether the operation can be constraint or not
};

/// Parameters describing a texture sampler
struct MetaTexture {
    Sampler sampler;
    Node array;
    Node depth_compare;
    std::vector<Node> aoffi;
    std::vector<Node> ptp;
    std::vector<Node> derivates;
    Node bias;
    Node lod;
    Node component;
    u32 element{};
    Node index;
};

struct MetaImage {
    const Image& image;
    std::vector<Node> values;
    u32 element{};
};

/// Parameters that modify an operation but are not part of any particular operand
using Meta =
    std::variant<MetaArithmetic, MetaTexture, MetaImage, MetaStackClass, Tegra::Shader::HalfType>;

class AmendNode {
public:
    std::optional<std::size_t> GetAmendIndex() const {
        if (amend_index == amend_null_index) {
            return std::nullopt;
        }
        return {amend_index};
    }

    void SetAmendIndex(std::size_t index) {
        amend_index = index;
    }

    void ClearAmend() {
        amend_index = amend_null_index;
    }

private:
    static constexpr std::size_t amend_null_index = 0xFFFFFFFFFFFFFFFFULL;
    std::size_t amend_index{amend_null_index};
};

/// Holds any kind of operation that can be done in the IR
class OperationNode final : public AmendNode {
public:
    explicit OperationNode(OperationCode code) : OperationNode(code, Meta{}) {}

    explicit OperationNode(OperationCode code, Meta meta)
        : OperationNode(code, std::move(meta), std::vector<Node>{}) {}

    explicit OperationNode(OperationCode code, std::vector<Node> operands)
        : OperationNode(code, Meta{}, std::move(operands)) {}

    explicit OperationNode(OperationCode code, Meta meta, std::vector<Node> operands)
        : code{code}, meta{std::move(meta)}, operands{std::move(operands)} {}

    template <typename... Args>
    explicit OperationNode(OperationCode code, Meta meta, Args&&... operands)
        : code{code}, meta{std::move(meta)}, operands{operands...} {}

    OperationCode GetCode() const {
        return code;
    }

    const Meta& GetMeta() const {
        return meta;
    }

    std::size_t GetOperandsCount() const {
        return operands.size();
    }

    const Node& operator[](std::size_t operand_index) const {
        return operands.at(operand_index);
    }

private:
    OperationCode code{};
    Meta meta{};
    std::vector<Node> operands;
};

/// Encloses inside any kind of node that returns a boolean conditionally-executed code
class ConditionalNode final : public AmendNode {
public:
    explicit ConditionalNode(Node condition, std::vector<Node>&& code)
        : condition{std::move(condition)}, code{std::move(code)} {}

    const Node& GetCondition() const {
        return condition;
    }

    const std::vector<Node>& GetCode() const {
        return code;
    }

private:
    Node condition;         ///< Condition to be satisfied
    std::vector<Node> code; ///< Code to execute
};

/// A general purpose register
class GprNode final {
public:
    explicit constexpr GprNode(Tegra::Shader::Register index) : index{index} {}

    u32 GetIndex() const {
        return static_cast<u32>(index);
    }

private:
    Tegra::Shader::Register index{};
};

/// A custom variable
class CustomVarNode final {
public:
    explicit constexpr CustomVarNode(u32 index) : index{index} {}

    constexpr u32 GetIndex() const {
        return index;
    }

private:
    u32 index{};
};

/// A 32-bits value that represents an immediate value
class ImmediateNode final {
public:
    explicit constexpr ImmediateNode(u32 value) : value{value} {}

    u32 GetValue() const {
        return value;
    }

private:
    u32 value{};
};

/// One of Maxwell's internal flags
class InternalFlagNode final {
public:
    explicit constexpr InternalFlagNode(InternalFlag flag) : flag{flag} {}

    InternalFlag GetFlag() const {
        return flag;
    }

private:
    InternalFlag flag{};
};

/// A predicate register, it can be negated without additional nodes
class PredicateNode final {
public:
    explicit constexpr PredicateNode(Tegra::Shader::Pred index, bool negated)
        : index{index}, negated{negated} {}

    Tegra::Shader::Pred GetIndex() const {
        return index;
    }

    bool IsNegated() const {
        return negated;
    }

private:
    Tegra::Shader::Pred index{};
    bool negated{};
};

/// Attribute buffer memory (known as attributes or varyings in GLSL terms)
class AbufNode final {
public:
    // Initialize for standard attributes (index is explicit).
    explicit AbufNode(Tegra::Shader::Attribute::Index index, u32 element, Node buffer = {})
        : buffer{std::move(buffer)}, index{index}, element{element} {}

    // Initialize for physical attributes (index is a variable value).
    explicit AbufNode(Node physical_address, Node buffer = {})
        : physical_address{std::move(physical_address)}, buffer{std::move(buffer)} {}

    Tegra::Shader::Attribute::Index GetIndex() const {
        return index;
    }

    u32 GetElement() const {
        return element;
    }

    const Node& GetBuffer() const {
        return buffer;
    }

    bool IsPhysicalBuffer() const {
        return static_cast<bool>(physical_address);
    }

    const Node& GetPhysicalAddress() const {
        return physical_address;
    }

private:
    Node physical_address;
    Node buffer;
    Tegra::Shader::Attribute::Index index{};
    u32 element{};
};

/// Patch memory (used to communicate tessellation stages).
class PatchNode final {
public:
    explicit PatchNode(u32 offset) : offset{offset} {}

    u32 GetOffset() const {
        return offset;
    }

private:
    u32 offset{};
};

/// Constant buffer node, usually mapped to uniform buffers in GLSL
class CbufNode final {
public:
    explicit CbufNode(u32 index, Node offset) : index{index}, offset{std::move(offset)} {}

    u32 GetIndex() const {
        return index;
    }

    const Node& GetOffset() const {
        return offset;
    }

private:
    u32 index{};
    Node offset;
};

/// Local memory node
class LmemNode final {
public:
    explicit LmemNode(Node address) : address{std::move(address)} {}

    const Node& GetAddress() const {
        return address;
    }

private:
    Node address;
};

/// Shared memory node
class SmemNode final {
public:
    explicit SmemNode(Node address) : address{std::move(address)} {}

    const Node& GetAddress() const {
        return address;
    }

private:
    Node address;
};

/// Global memory node
class GmemNode final {
public:
    explicit GmemNode(Node real_address, Node base_address, const GlobalMemoryBase& descriptor)
        : real_address{std::move(real_address)}, base_address{std::move(base_address)},
          descriptor{descriptor} {}

    const Node& GetRealAddress() const {
        return real_address;
    }

    const Node& GetBaseAddress() const {
        return base_address;
    }

    const GlobalMemoryBase& GetDescriptor() const {
        return descriptor;
    }

private:
    Node real_address;
    Node base_address;
    GlobalMemoryBase descriptor;
};

/// Commentary, can be dropped
class CommentNode final {
public:
    explicit CommentNode(std::string text) : text{std::move(text)} {}

    const std::string& GetText() const {
        return text;
    }

private:
    std::string text;
};

} // namespace VideoCommon::Shader
