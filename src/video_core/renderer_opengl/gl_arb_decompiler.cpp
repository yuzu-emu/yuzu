// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include <fmt/format.h>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/renderer_opengl/gl_arb_decompiler.h"
#include "video_core/renderer_opengl/gl_device.h"
#include "video_core/shader/registry.h"
#include "video_core/shader/shader_ir.h"

// Predicates in the decompiled code follow the convention that -1 means true and 0 means false.
// GLASM lacks booleans, so they have to be implemented as integers.
// Using -1 for true is useful because both CMP.S and NOT.U can negate it, and CMP.S can be used to
// select between two values, because -1 will be evaluated as true and 0 as false.

namespace OpenGL {

namespace {

using Tegra::Engines::ShaderType;
using Tegra::Shader::Attribute;
using Tegra::Shader::PixelImap;
using Tegra::Shader::Register;
using namespace VideoCommon::Shader;
using Operation = const OperationNode&;

constexpr std::array INTERNAL_FLAG_NAMES = {"ZERO", "SIGN", "CARRY", "OVERFLOW"};

char Swizzle(std::size_t component) {
    static constexpr std::string_view SWIZZLE{"xyzw"};
    return SWIZZLE.at(component);
}

constexpr bool IsGenericAttribute(Attribute::Index index) {
    return index >= Attribute::Index::Attribute_0 && index <= Attribute::Index::Attribute_31;
}

u32 GetGenericAttributeIndex(Attribute::Index index) {
    ASSERT(IsGenericAttribute(index));
    return static_cast<u32>(index) - static_cast<u32>(Attribute::Index::Attribute_0);
}

std::string_view Modifiers(Operation operation) {
    const auto meta = std::get_if<MetaArithmetic>(&operation.GetMeta());
    if (meta && meta->precise) {
        return ".PREC";
    }
    return "";
}

std::string_view GetInputFlags(PixelImap attribute) {
    switch (attribute) {
    case PixelImap::Perspective:
        return "";
    case PixelImap::Constant:
        return "FLAT ";
    case PixelImap::ScreenLinear:
        return "NOPERSPECTIVE ";
    case PixelImap::Unused:
        break;
    }
    UNIMPLEMENTED_MSG("Unknown attribute usage index={}", static_cast<int>(attribute));
    return {};
}

std::string_view ImageType(Tegra::Shader::ImageType image_type) {
    switch (image_type) {
    case Tegra::Shader::ImageType::Texture1D:
        return "1D";
    case Tegra::Shader::ImageType::TextureBuffer:
        return "BUFFER";
    case Tegra::Shader::ImageType::Texture1DArray:
        return "ARRAY1D";
    case Tegra::Shader::ImageType::Texture2D:
        return "2D";
    case Tegra::Shader::ImageType::Texture2DArray:
        return "ARRAY2D";
    case Tegra::Shader::ImageType::Texture3D:
        return "3D";
    }
    UNREACHABLE();
    return {};
}

std::string_view StackName(MetaStackClass stack) {
    switch (stack) {
    case MetaStackClass::Ssy:
        return "SSY";
    case MetaStackClass::Pbk:
        return "PBK";
    }
    UNREACHABLE();
    return "";
};

std::string_view PrimitiveDescription(Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology topology) {
    switch (topology) {
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::Points:
        return "POINTS";
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::Lines:
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::LineStrip:
        return "LINES";
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::LinesAdjacency:
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::LineStripAdjacency:
        return "LINES_ADJACENCY";
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::Triangles:
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::TriangleStrip:
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::TriangleFan:
        return "TRIANGLES";
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::TrianglesAdjacency:
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::TriangleStripAdjacency:
        return "TRIANGLES_ADJACENCY";
    default:
        UNIMPLEMENTED_MSG("topology={}", static_cast<int>(topology));
        return "POINTS";
    }
}

std::string_view TopologyName(Tegra::Shader::OutputTopology topology) {
    switch (topology) {
    case Tegra::Shader::OutputTopology::PointList:
        return "POINTS";
    case Tegra::Shader::OutputTopology::LineStrip:
        return "LINE_STRIP";
    case Tegra::Shader::OutputTopology::TriangleStrip:
        return "TRIANGLE_STRIP";
    default:
        UNIMPLEMENTED_MSG("Unknown output topology: {}", static_cast<u32>(topology));
        return "points";
    }
}

std::string_view StageInputName(ShaderType stage) {
    switch (stage) {
    case ShaderType::Vertex:
    case ShaderType::Geometry:
        return "vertex";
    case ShaderType::Fragment:
        return "fragment";
    case ShaderType::Compute:
        return "invocation";
    default:
        UNREACHABLE();
        return "";
    }
}

std::string TextureType(const MetaTexture& meta) {
    if (meta.sampler.is_buffer) {
        return "BUFFER";
    }
    std::string type;
    if (meta.sampler.is_shadow) {
        type += "SHADOW";
    }
    if (meta.sampler.is_array) {
        type += "ARRAY";
    }
    type += [&meta] {
        switch (meta.sampler.type) {
        case Tegra::Shader::TextureType::Texture1D:
            return "1D";
        case Tegra::Shader::TextureType::Texture2D:
            return "2D";
        case Tegra::Shader::TextureType::Texture3D:
            return "3D";
        case Tegra::Shader::TextureType::TextureCube:
            return "CUBE";
        }
        UNREACHABLE();
        return "2D";
    }();
    return type;
}

class ARBDecompiler final {
public:
    explicit ARBDecompiler(const Device& device, const ShaderIR& ir, const Registry& registry,
                           ShaderType stage, std::string_view identifier);

    std::string Code() const {
        return shader_source;
    }

private:
    void DefineGlobalMemory();

    void DeclareHeader();
    void DeclareVertex();
    void DeclareGeometry();
    void DeclareFragment();
    void DeclareCompute();
    void DeclareInputAttributes();
    void DeclareOutputAttributes();
    void DeclareLocalMemory();
    void DeclareGlobalMemory();
    void DeclareConstantBuffers();
    void DeclareRegisters();
    void DeclareTemporaries();
    void DeclarePredicates();
    void DeclareInternalFlags();

    void InitializeVariables();

    void DecompileAST();
    void DecompileBranchMode();

    void VisitAST(const ASTNode& node);
    std::string VisitExpression(const Expr& node);

    void VisitBlock(const NodeBlock& bb);

    std::string Visit(const Node& node);

    std::tuple<std::string, std::string, std::size_t> BuildCoords(Operation);
    std::string BuildAoffi(Operation);
    std::string GlobalMemoryPointer(const GmemNode& gmem);
    void Exit();

    std::string Assign(Operation);
    std::string Select(Operation);
    std::string FClamp(Operation);
    std::string FCastHalf0(Operation);
    std::string FCastHalf1(Operation);
    std::string FSqrt(Operation);
    std::string FSwizzleAdd(Operation);
    std::string HAdd2(Operation);
    std::string HMul2(Operation);
    std::string HFma2(Operation);
    std::string HAbsolute(Operation);
    std::string HNegate(Operation);
    std::string HClamp(Operation);
    std::string HCastFloat(Operation);
    std::string HUnpack(Operation);
    std::string HMergeF32(Operation);
    std::string HMergeH0(Operation);
    std::string HMergeH1(Operation);
    std::string HPack2(Operation);
    std::string LogicalAssign(Operation);
    std::string LogicalPick2(Operation);
    std::string LogicalAnd2(Operation);
    std::string FloatOrdered(Operation);
    std::string FloatUnordered(Operation);
    std::string LogicalAddCarry(Operation);
    std::string Texture(Operation);
    std::string TextureGather(Operation);
    std::string TextureQueryDimensions(Operation);
    std::string TextureQueryLod(Operation);
    std::string TexelFetch(Operation);
    std::string TextureGradient(Operation);
    std::string ImageLoad(Operation);
    std::string ImageStore(Operation);
    std::string Branch(Operation);
    std::string BranchIndirect(Operation);
    std::string PushFlowStack(Operation);
    std::string PopFlowStack(Operation);
    std::string Exit(Operation);
    std::string Discard(Operation);
    std::string EmitVertex(Operation);
    std::string EndPrimitive(Operation);
    std::string InvocationId(Operation);
    std::string YNegate(Operation);
    std::string ThreadId(Operation);
    std::string ShuffleIndexed(Operation);
    std::string Barrier(Operation);
    std::string MemoryBarrierGroup(Operation);
    std::string MemoryBarrierGlobal(Operation);

    template <const std::string_view& op>
    std::string Unary(Operation operation) {
        std::string temporary = AllocTemporary();
        AddLine("{}{} {}, {};", op, Modifiers(operation), temporary, Visit(operation[0]));
        return temporary;
    }

    template <const std::string_view& op>
    std::string Binary(Operation operation) {
        std::string temporary = AllocTemporary();
        AddLine("{}{} {}, {}, {};", op, Modifiers(operation), temporary, Visit(operation[0]),
                Visit(operation[1]));
        return temporary;
    }

    template <const std::string_view& op>
    std::string Trinary(Operation operation) {
        std::string temporary = AllocTemporary();
        AddLine("{}{} {}, {}, {}, {};", op, Modifiers(operation), temporary, Visit(operation[0]),
                Visit(operation[1]), Visit(operation[2]));
        return temporary;
    }

    template <const std::string_view& op, bool unordered>
    std::string FloatComparison(Operation operation) {
        std::string temporary = AllocTemporary();
        AddLine("TRUNC.U.CC RC.x, {};", Binary<op>(operation));
        AddLine("MOV.S {}, 0;", temporary);
        AddLine("MOV.S {} (NE.x), -1;", temporary);

        const std::string op_a = Visit(operation[0]);
        const std::string op_b = Visit(operation[1]);
        if constexpr (unordered) {
            AddLine("SNE.F RC.x, {}, {};", op_a, op_a);
            AddLine("TRUNC.U.CC RC.x, RC.x;");
            AddLine("MOV.S {} (NE.x), -1;", temporary);
            AddLine("SNE.F RC.x, {}, {};", op_b, op_b);
            AddLine("TRUNC.U.CC RC.x, RC.x;");
            AddLine("MOV.S {} (NE.x), -1;", temporary);
        } else if (op == SNE_F) {
            AddLine("SNE.F RC.x, {}, {};", op_a, op_a);
            AddLine("TRUNC.U.CC RC.x, RC.x;");
            AddLine("MOV.S {} (NE.x), 0;", temporary);
            AddLine("SNE.F RC.x, {}, {};", op_b, op_b);
            AddLine("TRUNC.U.CC RC.x, RC.x;");
            AddLine("MOV.S {} (NE.x), 0;", temporary);
        }
        return temporary;
    }

    template <const std::string_view& op, bool is_nan>
    std::string HalfComparison(Operation operation) {
        std::string tmp1 = AllocVectorTemporary();
        const std::string tmp2 = AllocVectorTemporary();
        const std::string op_a = Visit(operation[0]);
        const std::string op_b = Visit(operation[1]);
        AddLine("UP2H.F {}, {};", tmp1, op_a);
        AddLine("UP2H.F {}, {};", tmp2, op_b);
        AddLine("{} {}, {}, {};", op, tmp1, tmp1, tmp2);
        AddLine("TRUNC.U.CC RC.xy, {};", tmp1);
        AddLine("MOV.S {}.xy, {{0, 0, 0, 0}};", tmp1);
        AddLine("MOV.S {}.x (NE.x), -1;", tmp1);
        AddLine("MOV.S {}.y (NE.y), -1;", tmp1);
        if constexpr (is_nan) {
            AddLine("MOVC.F RC.x, {};", op_a);
            AddLine("MOV.S {}.x (NAN.x), -1;", tmp1);
            AddLine("MOVC.F RC.x, {};", op_b);
            AddLine("MOV.S {}.y (NAN.x), -1;", tmp1);
        }
        return tmp1;
    }

    template <const std::string_view& op, const std::string_view& type>
    std::string AtomicImage(Operation operation) {
        const auto& meta = std::get<MetaImage>(operation.GetMeta());
        const u32 image_id = device.GetBaseBindings(stage).image + meta.image.index;
        const std::size_t num_coords = operation.GetOperandsCount();
        const std::size_t num_values = meta.values.size();

        const std::string coord = AllocVectorTemporary();
        const std::string value = AllocVectorTemporary();
        for (std::size_t i = 0; i < num_coords; ++i) {
            AddLine("MOV.S {}.{}, {};", coord, Swizzle(i), Visit(operation[i]));
        }
        for (std::size_t i = 0; i < num_values; ++i) {
            AddLine("MOV.F {}.{}, {};", value, Swizzle(i), Visit(meta.values[i]));
        }

        AddLine("ATOMIM.{}.{} {}.x, {}, {}, image[{}], {};", op, type, coord, value, coord,
                image_id, ImageType(meta.image.type));
        return fmt::format("{}.x", coord);
    }

    template <const std::string_view& op, const std::string_view& type>
    std::string Atomic(Operation operation) {
        std::string temporary = AllocTemporary();
        std::string address;
        std::string_view opname;
        bool robust = false;
        if (const auto gmem = std::get_if<GmemNode>(&*operation[0])) {
            address = GlobalMemoryPointer(*gmem);
            opname = "ATOM";
            robust = true;
        } else if (const auto smem = std::get_if<SmemNode>(&*operation[0])) {
            address = fmt::format("shared_mem[{}]", Visit(smem->GetAddress()));
            opname = "ATOMS";
        } else {
            UNREACHABLE();
            return "{0, 0, 0, 0}";
        }
        if (robust) {
            AddLine("IF NE.x;");
        }
        AddLine("{}.{}.{} {}, {}, {};", opname, op, type, temporary, Visit(operation[1]), address);
        if (robust) {
            AddLine("ELSE;");
            AddLine("MOV.S {}, 0;", temporary);
            AddLine("ENDIF;");
        }
        return temporary;
    }

    template <char type>
    std::string Negate(Operation operation) {
        std::string temporary = AllocTemporary();
        if constexpr (type == 'F') {
            AddLine("MOV.F32 {}, -{};", temporary, Visit(operation[0]));
        } else {
            AddLine("MOV.{} {}, -{};", type, temporary, Visit(operation[0]));
        }
        return temporary;
    }

    template <char type>
    std::string Absolute(Operation operation) {
        std::string temporary = AllocTemporary();
        AddLine("MOV.{} {}, |{}|;", type, temporary, Visit(operation[0]));
        return temporary;
    }

    template <char type>
    std::string BitfieldInsert(Operation operation) {
        const std::string temporary = AllocVectorTemporary();
        AddLine("MOV.{} {}.x, {};", type, temporary, Visit(operation[3]));
        AddLine("MOV.{} {}.y, {};", type, temporary, Visit(operation[2]));
        AddLine("BFI.{} {}.x, {}, {}, {};", type, temporary, temporary, Visit(operation[1]),
                Visit(operation[0]));
        return fmt::format("{}.x", temporary);
    }

    template <char type>
    std::string BitfieldExtract(Operation operation) {
        const std::string temporary = AllocVectorTemporary();
        AddLine("MOV.{} {}.x, {};", type, temporary, Visit(operation[2]));
        AddLine("MOV.{} {}.y, {};", type, temporary, Visit(operation[1]));
        AddLine("BFE.{} {}.x, {}, {};", type, temporary, temporary, Visit(operation[0]));
        return fmt::format("{}.x", temporary);
    }

    template <char swizzle>
    std::string LocalInvocationId(Operation) {
        return fmt::format("invocation.localid.{}", swizzle);
    }

    template <char swizzle>
    std::string WorkGroupId(Operation) {
        return fmt::format("invocation.groupid.{}", swizzle);
    }

    template <char c1, char c2>
    std::string ThreadMask(Operation) {
        return fmt::format("{}.thread{}{}mask", StageInputName(stage), c1, c2);
    }

    template <typename... Args>
    void AddExpression(std::string_view text, Args&&... args) {
        shader_source += fmt::format(text, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void AddLine(std::string_view text, Args&&... args) {
        AddExpression(text, std::forward<Args>(args)...);
        shader_source += '\n';
    }

    std::string AllocLongVectorTemporary() {
        max_long_temporaries = std::max(max_long_temporaries, num_long_temporaries + 1);
        return fmt::format("L{}", num_long_temporaries++);
    }

    std::string AllocLongTemporary() {
        return fmt::format("{}.x", AllocLongVectorTemporary());
    }

    std::string AllocVectorTemporary() {
        max_temporaries = std::max(max_temporaries, num_temporaries + 1);
        return fmt::format("T{}", num_temporaries++);
    }

    std::string AllocTemporary() {
        return fmt::format("{}.x", AllocVectorTemporary());
    }

    void ResetTemporaries() noexcept {
        num_temporaries = 0;
        num_long_temporaries = 0;
    }

    const Device& device;
    const ShaderIR& ir;
    const Registry& registry;
    const ShaderType stage;

    std::size_t num_temporaries = 0;
    std::size_t max_temporaries = 0;

    std::size_t num_long_temporaries = 0;
    std::size_t max_long_temporaries = 0;

    std::map<GlobalMemoryBase, u32> global_memory_names;

    std::string shader_source;

    static constexpr std::string_view ADD_F32 = "ADD.F32";
    static constexpr std::string_view ADD_S = "ADD.S";
    static constexpr std::string_view ADD_U = "ADD.U";
    static constexpr std::string_view MUL_F32 = "MUL.F32";
    static constexpr std::string_view MUL_S = "MUL.S";
    static constexpr std::string_view MUL_U = "MUL.U";
    static constexpr std::string_view DIV_F32 = "DIV.F32";
    static constexpr std::string_view DIV_S = "DIV.S";
    static constexpr std::string_view DIV_U = "DIV.U";
    static constexpr std::string_view MAD_F32 = "MAD.F32";
    static constexpr std::string_view RSQ_F32 = "RSQ.F32";
    static constexpr std::string_view COS_F32 = "COS.F32";
    static constexpr std::string_view SIN_F32 = "SIN.F32";
    static constexpr std::string_view EX2_F32 = "EX2.F32";
    static constexpr std::string_view LG2_F32 = "LG2.F32";
    static constexpr std::string_view SLT_F = "SLT.F32";
    static constexpr std::string_view SLT_S = "SLT.S";
    static constexpr std::string_view SLT_U = "SLT.U";
    static constexpr std::string_view SEQ_F = "SEQ.F32";
    static constexpr std::string_view SEQ_S = "SEQ.S";
    static constexpr std::string_view SEQ_U = "SEQ.U";
    static constexpr std::string_view SLE_F = "SLE.F32";
    static constexpr std::string_view SLE_S = "SLE.S";
    static constexpr std::string_view SLE_U = "SLE.U";
    static constexpr std::string_view SGT_F = "SGT.F32";
    static constexpr std::string_view SGT_S = "SGT.S";
    static constexpr std::string_view SGT_U = "SGT.U";
    static constexpr std::string_view SNE_F = "SNE.F32";
    static constexpr std::string_view SNE_S = "SNE.S";
    static constexpr std::string_view SNE_U = "SNE.U";
    static constexpr std::string_view SGE_F = "SGE.F32";
    static constexpr std::string_view SGE_S = "SGE.S";
    static constexpr std::string_view SGE_U = "SGE.U";
    static constexpr std::string_view AND_S = "AND.S";
    static constexpr std::string_view AND_U = "AND.U";
    static constexpr std::string_view TRUNC_F = "TRUNC.F";
    static constexpr std::string_view TRUNC_S = "TRUNC.S";
    static constexpr std::string_view TRUNC_U = "TRUNC.U";
    static constexpr std::string_view SHL_S = "SHL.S";
    static constexpr std::string_view SHL_U = "SHL.U";
    static constexpr std::string_view SHR_S = "SHR.S";
    static constexpr std::string_view SHR_U = "SHR.U";
    static constexpr std::string_view OR_S = "OR.S";
    static constexpr std::string_view OR_U = "OR.U";
    static constexpr std::string_view XOR_S = "XOR.S";
    static constexpr std::string_view XOR_U = "XOR.U";
    static constexpr std::string_view NOT_S = "NOT.S";
    static constexpr std::string_view NOT_U = "NOT.U";
    static constexpr std::string_view BTC_S = "BTC.S";
    static constexpr std::string_view BTC_U = "BTC.U";
    static constexpr std::string_view BTFM_S = "BTFM.S";
    static constexpr std::string_view BTFM_U = "BTFM.U";
    static constexpr std::string_view ROUND_F = "ROUND.F";
    static constexpr std::string_view CEIL_F = "CEIL.F";
    static constexpr std::string_view FLR_F = "FLR.F";
    static constexpr std::string_view I2F_S = "I2F.S";
    static constexpr std::string_view I2F_U = "I2F.U";
    static constexpr std::string_view MIN_F = "MIN.F";
    static constexpr std::string_view MIN_S = "MIN.S";
    static constexpr std::string_view MIN_U = "MIN.U";
    static constexpr std::string_view MAX_F = "MAX.F";
    static constexpr std::string_view MAX_S = "MAX.S";
    static constexpr std::string_view MAX_U = "MAX.U";
    static constexpr std::string_view MOV_U = "MOV.U";
    static constexpr std::string_view TGBALLOT_U = "TGBALLOT.U";
    static constexpr std::string_view TGALL_U = "TGALL.U";
    static constexpr std::string_view TGANY_U = "TGANY.U";
    static constexpr std::string_view TGEQ_U = "TGEQ.U";
    static constexpr std::string_view EXCH = "EXCH";
    static constexpr std::string_view ADD = "ADD";
    static constexpr std::string_view MIN = "MIN";
    static constexpr std::string_view MAX = "MAX";
    static constexpr std::string_view AND = "AND";
    static constexpr std::string_view OR = "OR";
    static constexpr std::string_view XOR = "XOR";
    static constexpr std::string_view U32 = "U32";
    static constexpr std::string_view S32 = "S32";

    static constexpr std::size_t NUM_ENTRIES = static_cast<std::size_t>(OperationCode::Amount);
    using DecompilerType = std::string (ARBDecompiler::*)(Operation);
    static constexpr std::array<DecompilerType, NUM_ENTRIES> OPERATION_DECOMPILERS = {
        &ARBDecompiler::Assign,

        &ARBDecompiler::Select,

        &ARBDecompiler::Binary<ADD_F32>,
        &ARBDecompiler::Binary<MUL_F32>,
        &ARBDecompiler::Binary<DIV_F32>,
        &ARBDecompiler::Trinary<MAD_F32>,
        &ARBDecompiler::Negate<'F'>,
        &ARBDecompiler::Absolute<'F'>,
        &ARBDecompiler::FClamp,
        &ARBDecompiler::FCastHalf0,
        &ARBDecompiler::FCastHalf1,
        &ARBDecompiler::Binary<MIN_F>,
        &ARBDecompiler::Binary<MAX_F>,
        &ARBDecompiler::Unary<COS_F32>,
        &ARBDecompiler::Unary<SIN_F32>,
        &ARBDecompiler::Unary<EX2_F32>,
        &ARBDecompiler::Unary<LG2_F32>,
        &ARBDecompiler::Unary<RSQ_F32>,
        &ARBDecompiler::FSqrt,
        &ARBDecompiler::Unary<ROUND_F>,
        &ARBDecompiler::Unary<FLR_F>,
        &ARBDecompiler::Unary<CEIL_F>,
        &ARBDecompiler::Unary<TRUNC_F>,
        &ARBDecompiler::Unary<I2F_S>,
        &ARBDecompiler::Unary<I2F_U>,
        &ARBDecompiler::FSwizzleAdd,

        &ARBDecompiler::Binary<ADD_S>,
        &ARBDecompiler::Binary<MUL_S>,
        &ARBDecompiler::Binary<DIV_S>,
        &ARBDecompiler::Negate<'S'>,
        &ARBDecompiler::Absolute<'S'>,
        &ARBDecompiler::Binary<MIN_S>,
        &ARBDecompiler::Binary<MAX_S>,

        &ARBDecompiler::Unary<TRUNC_S>,
        &ARBDecompiler::Unary<MOV_U>,
        &ARBDecompiler::Binary<SHL_S>,
        &ARBDecompiler::Binary<SHR_U>,
        &ARBDecompiler::Binary<SHR_S>,
        &ARBDecompiler::Binary<AND_S>,
        &ARBDecompiler::Binary<OR_S>,
        &ARBDecompiler::Binary<XOR_S>,
        &ARBDecompiler::Unary<NOT_S>,
        &ARBDecompiler::BitfieldInsert<'S'>,
        &ARBDecompiler::BitfieldExtract<'S'>,
        &ARBDecompiler::Unary<BTC_S>,
        &ARBDecompiler::Unary<BTFM_S>,

        &ARBDecompiler::Binary<ADD_U>,
        &ARBDecompiler::Binary<MUL_U>,
        &ARBDecompiler::Binary<DIV_U>,
        &ARBDecompiler::Binary<MIN_U>,
        &ARBDecompiler::Binary<MAX_U>,
        &ARBDecompiler::Unary<TRUNC_U>,
        &ARBDecompiler::Unary<MOV_U>,
        &ARBDecompiler::Binary<SHL_U>,
        &ARBDecompiler::Binary<SHR_U>,
        &ARBDecompiler::Binary<SHR_U>,
        &ARBDecompiler::Binary<AND_U>,
        &ARBDecompiler::Binary<OR_U>,
        &ARBDecompiler::Binary<XOR_U>,
        &ARBDecompiler::Unary<NOT_U>,
        &ARBDecompiler::BitfieldInsert<'U'>,
        &ARBDecompiler::BitfieldExtract<'U'>,
        &ARBDecompiler::Unary<BTC_U>,
        &ARBDecompiler::Unary<BTFM_U>,

        &ARBDecompiler::HAdd2,
        &ARBDecompiler::HMul2,
        &ARBDecompiler::HFma2,
        &ARBDecompiler::HAbsolute,
        &ARBDecompiler::HNegate,
        &ARBDecompiler::HClamp,
        &ARBDecompiler::HCastFloat,
        &ARBDecompiler::HUnpack,
        &ARBDecompiler::HMergeF32,
        &ARBDecompiler::HMergeH0,
        &ARBDecompiler::HMergeH1,
        &ARBDecompiler::HPack2,

        &ARBDecompiler::LogicalAssign,
        &ARBDecompiler::Binary<AND_U>,
        &ARBDecompiler::Binary<OR_U>,
        &ARBDecompiler::Binary<XOR_U>,
        &ARBDecompiler::Unary<NOT_U>,
        &ARBDecompiler::LogicalPick2,
        &ARBDecompiler::LogicalAnd2,

        &ARBDecompiler::FloatComparison<SLT_F, false>,
        &ARBDecompiler::FloatComparison<SEQ_F, false>,
        &ARBDecompiler::FloatComparison<SLE_F, false>,
        &ARBDecompiler::FloatComparison<SGT_F, false>,
        &ARBDecompiler::FloatComparison<SNE_F, false>,
        &ARBDecompiler::FloatComparison<SGE_F, false>,
        &ARBDecompiler::FloatOrdered,
        &ARBDecompiler::FloatUnordered,
        &ARBDecompiler::FloatComparison<SLT_F, true>,
        &ARBDecompiler::FloatComparison<SEQ_F, true>,
        &ARBDecompiler::FloatComparison<SLE_F, true>,
        &ARBDecompiler::FloatComparison<SGT_F, true>,
        &ARBDecompiler::FloatComparison<SNE_F, true>,
        &ARBDecompiler::FloatComparison<SGE_F, true>,

        &ARBDecompiler::Binary<SLT_S>,
        &ARBDecompiler::Binary<SEQ_S>,
        &ARBDecompiler::Binary<SLE_S>,
        &ARBDecompiler::Binary<SGT_S>,
        &ARBDecompiler::Binary<SNE_S>,
        &ARBDecompiler::Binary<SGE_S>,

        &ARBDecompiler::Binary<SLT_U>,
        &ARBDecompiler::Binary<SEQ_U>,
        &ARBDecompiler::Binary<SLE_U>,
        &ARBDecompiler::Binary<SGT_U>,
        &ARBDecompiler::Binary<SNE_U>,
        &ARBDecompiler::Binary<SGE_U>,

        &ARBDecompiler::LogicalAddCarry,

        &ARBDecompiler::HalfComparison<SLT_F, false>,
        &ARBDecompiler::HalfComparison<SEQ_F, false>,
        &ARBDecompiler::HalfComparison<SLE_F, false>,
        &ARBDecompiler::HalfComparison<SGT_F, false>,
        &ARBDecompiler::HalfComparison<SNE_F, false>,
        &ARBDecompiler::HalfComparison<SGE_F, false>,
        &ARBDecompiler::HalfComparison<SLT_F, true>,
        &ARBDecompiler::HalfComparison<SEQ_F, true>,
        &ARBDecompiler::HalfComparison<SLE_F, true>,
        &ARBDecompiler::HalfComparison<SGT_F, true>,
        &ARBDecompiler::HalfComparison<SNE_F, true>,
        &ARBDecompiler::HalfComparison<SGE_F, true>,

        &ARBDecompiler::Texture,
        &ARBDecompiler::Texture,
        &ARBDecompiler::TextureGather,
        &ARBDecompiler::TextureQueryDimensions,
        &ARBDecompiler::TextureQueryLod,
        &ARBDecompiler::TexelFetch,
        &ARBDecompiler::TextureGradient,

        &ARBDecompiler::ImageLoad,
        &ARBDecompiler::ImageStore,

        &ARBDecompiler::AtomicImage<ADD, U32>,
        &ARBDecompiler::AtomicImage<AND, U32>,
        &ARBDecompiler::AtomicImage<OR, U32>,
        &ARBDecompiler::AtomicImage<XOR, U32>,
        &ARBDecompiler::AtomicImage<EXCH, U32>,

        &ARBDecompiler::Atomic<EXCH, U32>,
        &ARBDecompiler::Atomic<ADD, U32>,
        &ARBDecompiler::Atomic<MIN, U32>,
        &ARBDecompiler::Atomic<MAX, U32>,
        &ARBDecompiler::Atomic<AND, U32>,
        &ARBDecompiler::Atomic<OR, U32>,
        &ARBDecompiler::Atomic<XOR, U32>,

        &ARBDecompiler::Atomic<EXCH, S32>,
        &ARBDecompiler::Atomic<ADD, S32>,
        &ARBDecompiler::Atomic<MIN, S32>,
        &ARBDecompiler::Atomic<MAX, S32>,
        &ARBDecompiler::Atomic<AND, S32>,
        &ARBDecompiler::Atomic<OR, S32>,
        &ARBDecompiler::Atomic<XOR, S32>,

        &ARBDecompiler::Atomic<ADD, U32>,
        &ARBDecompiler::Atomic<MIN, U32>,
        &ARBDecompiler::Atomic<MAX, U32>,
        &ARBDecompiler::Atomic<AND, U32>,
        &ARBDecompiler::Atomic<OR, U32>,
        &ARBDecompiler::Atomic<XOR, U32>,

        &ARBDecompiler::Atomic<ADD, S32>,
        &ARBDecompiler::Atomic<MIN, S32>,
        &ARBDecompiler::Atomic<MAX, S32>,
        &ARBDecompiler::Atomic<AND, S32>,
        &ARBDecompiler::Atomic<OR, S32>,
        &ARBDecompiler::Atomic<XOR, S32>,

        &ARBDecompiler::Branch,
        &ARBDecompiler::BranchIndirect,
        &ARBDecompiler::PushFlowStack,
        &ARBDecompiler::PopFlowStack,
        &ARBDecompiler::Exit,
        &ARBDecompiler::Discard,

        &ARBDecompiler::EmitVertex,
        &ARBDecompiler::EndPrimitive,

        &ARBDecompiler::InvocationId,
        &ARBDecompiler::YNegate,
        &ARBDecompiler::LocalInvocationId<'x'>,
        &ARBDecompiler::LocalInvocationId<'y'>,
        &ARBDecompiler::LocalInvocationId<'z'>,
        &ARBDecompiler::WorkGroupId<'x'>,
        &ARBDecompiler::WorkGroupId<'y'>,
        &ARBDecompiler::WorkGroupId<'z'>,

        &ARBDecompiler::Unary<TGBALLOT_U>,
        &ARBDecompiler::Unary<TGALL_U>,
        &ARBDecompiler::Unary<TGANY_U>,
        &ARBDecompiler::Unary<TGEQ_U>,

        &ARBDecompiler::ThreadId,
        &ARBDecompiler::ThreadMask<'e', 'q'>,
        &ARBDecompiler::ThreadMask<'g', 'e'>,
        &ARBDecompiler::ThreadMask<'g', 't'>,
        &ARBDecompiler::ThreadMask<'l', 'e'>,
        &ARBDecompiler::ThreadMask<'l', 't'>,
        &ARBDecompiler::ShuffleIndexed,

        &ARBDecompiler::Barrier,
        &ARBDecompiler::MemoryBarrierGroup,
        &ARBDecompiler::MemoryBarrierGlobal,
    };
};

ARBDecompiler::ARBDecompiler(const Device& device, const ShaderIR& ir, const Registry& registry,
                             ShaderType stage, std::string_view identifier)
    : device{device}, ir{ir}, registry{registry}, stage{stage} {
    DefineGlobalMemory();

    AddLine("TEMP RC;");
    AddLine("TEMP FSWZA[4];");
    AddLine("TEMP FSWZB[4];");
    if (ir.IsDecompiled()) {
        DecompileAST();
    } else {
        DecompileBranchMode();
    }
    AddLine("END");

    const std::string code = std::move(shader_source);
    DeclareHeader();
    DeclareVertex();
    DeclareGeometry();
    DeclareFragment();
    DeclareCompute();
    DeclareInputAttributes();
    DeclareOutputAttributes();
    DeclareLocalMemory();
    DeclareGlobalMemory();
    DeclareConstantBuffers();
    DeclareRegisters();
    DeclareTemporaries();
    DeclarePredicates();
    DeclareInternalFlags();

    shader_source += code;
}

std::string_view HeaderStageName(ShaderType stage) {
    switch (stage) {
    case ShaderType::Vertex:
        return "vp";
    case ShaderType::Geometry:
        return "gp";
    case ShaderType::Fragment:
        return "fp";
    case ShaderType::Compute:
        return "cp";
    default:
        UNREACHABLE();
        return "";
    }
}

void ARBDecompiler::DefineGlobalMemory() {
    u32 binding = 0;
    for (const auto& pair : ir.GetGlobalMemory()) {
        const GlobalMemoryBase base = pair.first;
        global_memory_names.emplace(base, binding);
        ++binding;
    }
}

void ARBDecompiler::DeclareHeader() {
    AddLine("!!NV{}5.0", HeaderStageName(stage));
    // Enabling this allows us to cheat on some instructions like TXL with SHADOWARRAY2D
    AddLine("OPTION NV_internal;");
    AddLine("OPTION NV_gpu_program_fp64;");
    AddLine("OPTION NV_shader_thread_group;");
    if (ir.UsesWarps() && device.HasWarpIntrinsics()) {
        AddLine("OPTION NV_shader_thread_shuffle;");
    }
    if (stage == ShaderType::Vertex) {
        if (device.HasNvViewportArray2()) {
            AddLine("OPTION NV_viewport_array2;");
        }
    }
    if (stage == ShaderType::Fragment) {
        AddLine("OPTION ARB_draw_buffers;");
    }
    if (device.HasImageLoadFormatted()) {
        AddLine("OPTION EXT_shader_image_load_formatted;");
    }
}

void ARBDecompiler::DeclareVertex() {
    if (stage != ShaderType::Vertex) {
        return;
    }
    AddLine("OUTPUT result_clip[] = {{ result.clip[0..7] }};");
}

void ARBDecompiler::DeclareGeometry() {
    if (stage != ShaderType::Geometry) {
        return;
    }
    const auto& info = registry.GetGraphicsInfo();
    const auto& header = ir.GetHeader();
    AddLine("PRIMITIVE_IN {};", PrimitiveDescription(info.primitive_topology));
    AddLine("PRIMITIVE_OUT {};", TopologyName(header.common3.output_topology));
    AddLine("VERTICES_OUT {};", header.common4.max_output_vertices.Value());
    AddLine("ATTRIB vertex_position = vertex.position;");
}

void ARBDecompiler::DeclareFragment() {
    if (stage != ShaderType::Fragment) {
        return;
    }
    AddLine("OUTPUT result_color7 = result.color[7];");
    AddLine("OUTPUT result_color6 = result.color[6];");
    AddLine("OUTPUT result_color5 = result.color[5];");
    AddLine("OUTPUT result_color4 = result.color[4];");
    AddLine("OUTPUT result_color3 = result.color[3];");
    AddLine("OUTPUT result_color2 = result.color[2];");
    AddLine("OUTPUT result_color1 = result.color[1];");
    AddLine("OUTPUT result_color0 = result.color;");
}

void ARBDecompiler::DeclareCompute() {
    if (stage != ShaderType::Compute) {
        return;
    }
    const ComputeInfo& info = registry.GetComputeInfo();
    AddLine("GROUP_SIZE {} {} {};", info.workgroup_size[0], info.workgroup_size[1],
            info.workgroup_size[2]);
    if (info.shared_memory_size_in_words == 0) {
        return;
    }
    const u32 limit = device.GetMaxComputeSharedMemorySize();
    u32 size_in_bytes = info.shared_memory_size_in_words * 4;
    if (size_in_bytes > limit) {
        LOG_ERROR(Render_OpenGL, "Shared memory size {} is clamped to host's limit {}",
                  size_in_bytes, limit);
        size_in_bytes = limit;
    }

    AddLine("SHARED_MEMORY {};", size_in_bytes);
    AddLine("SHARED shared_mem[] = {{program.sharedmem}};");
}

void ARBDecompiler::DeclareInputAttributes() {
    if (stage == ShaderType::Compute) {
        return;
    }
    const std::string_view stage_name = StageInputName(stage);
    for (const auto attribute : ir.GetInputAttributes()) {
        if (!IsGenericAttribute(attribute)) {
            continue;
        }
        const u32 index = GetGenericAttributeIndex(attribute);

        std::string_view suffix;
        if (stage == ShaderType::Fragment) {
            const auto input_mode{ir.GetHeader().ps.GetPixelImap(index)};
            if (input_mode == PixelImap::Unused) {
                return;
            }
            suffix = GetInputFlags(input_mode);
        }
        AddLine("{}ATTRIB in_attr{}[] = {{ {}.attrib[{}..{}] }};", suffix, index, stage_name, index,
                index);
    }
}

void ARBDecompiler::DeclareOutputAttributes() {
    if (stage == ShaderType::Compute) {
        return;
    }
    for (const auto attribute : ir.GetOutputAttributes()) {
        if (!IsGenericAttribute(attribute)) {
            continue;
        }
        const u32 index = GetGenericAttributeIndex(attribute);
        AddLine("OUTPUT out_attr{}[] = {{ result.attrib[{}..{}] }};", index, index, index);
    }
}

void ARBDecompiler::DeclareLocalMemory() {
    u64 size = 0;
    if (stage == ShaderType::Compute) {
        size = registry.GetComputeInfo().local_memory_size_in_words * 4ULL;
    } else {
        size = ir.GetHeader().GetLocalMemorySize();
    }
    if (size == 0) {
        return;
    }
    const u64 element_count = Common::AlignUp(size, 4) / 4;
    AddLine("TEMP lmem[{}];", element_count);
}

void ARBDecompiler::DeclareGlobalMemory() {
    const size_t num_entries = ir.GetGlobalMemory().size();
    if (num_entries > 0) {
        AddLine("PARAM c[{}] = {{ program.local[0..{}] }};", num_entries, num_entries - 1);
    }
}

void ARBDecompiler::DeclareConstantBuffers() {
    u32 binding = 0;
    for (const auto& cbuf : ir.GetConstantBuffers()) {
        AddLine("CBUFFER cbuf{}[] = {{ program.buffer[{}] }};", cbuf.first, binding);
        ++binding;
    }
}

void ARBDecompiler::DeclareRegisters() {
    for (const u32 gpr : ir.GetRegisters()) {
        AddLine("TEMP R{};", gpr);
    }
}

void ARBDecompiler::DeclareTemporaries() {
    for (std::size_t i = 0; i < max_temporaries; ++i) {
        AddLine("TEMP T{};", i);
    }
    for (std::size_t i = 0; i < max_long_temporaries; ++i) {
        AddLine("LONG TEMP L{};", i);
    }
}

void ARBDecompiler::DeclarePredicates() {
    for (const Tegra::Shader::Pred pred : ir.GetPredicates()) {
        AddLine("TEMP P{};", static_cast<u64>(pred));
    }
}

void ARBDecompiler::DeclareInternalFlags() {
    for (const char* name : INTERNAL_FLAG_NAMES) {
        AddLine("TEMP {};", name);
    }
}

void ARBDecompiler::InitializeVariables() {
    AddLine("MOV.F32 FSWZA[0], -1;");
    AddLine("MOV.F32 FSWZA[1], 1;");
    AddLine("MOV.F32 FSWZA[2], -1;");
    AddLine("MOV.F32 FSWZA[3], 0;");
    AddLine("MOV.F32 FSWZB[0], -1;");
    AddLine("MOV.F32 FSWZB[1], -1;");
    AddLine("MOV.F32 FSWZB[2], 1;");
    AddLine("MOV.F32 FSWZB[3], -1;");

    if (stage == ShaderType::Vertex || stage == ShaderType::Geometry) {
        AddLine("MOV.F result.position, {{0, 0, 0, 1}};");
    }
    for (const auto attribute : ir.GetOutputAttributes()) {
        if (!IsGenericAttribute(attribute)) {
            continue;
        }
        const u32 index = GetGenericAttributeIndex(attribute);
        AddLine("MOV.F result.attrib[{}], {{0, 0, 0, 1}};", index);
    }
    for (const u32 gpr : ir.GetRegisters()) {
        AddLine("MOV.F R{}, {{0, 0, 0, 0}};", gpr);
    }
    for (const Tegra::Shader::Pred pred : ir.GetPredicates()) {
        AddLine("MOV.U P{}, {{0, 0, 0, 0}};", static_cast<u64>(pred));
    }
}

void ARBDecompiler::DecompileAST() {
    const u32 num_flow_variables = ir.GetASTNumVariables();
    for (u32 i = 0; i < num_flow_variables; ++i) {
        AddLine("TEMP F{};", i);
    }
    for (u32 i = 0; i < num_flow_variables; ++i) {
        AddLine("MOV.U F{}, {{0, 0, 0, 0}};", i);
    }

    InitializeVariables();

    VisitAST(ir.GetASTProgram());
}

void ARBDecompiler::DecompileBranchMode() {
    static constexpr u32 FLOW_STACK_SIZE = 20;
    if (!ir.IsFlowStackDisabled()) {
        AddLine("TEMP SSY[{}];", FLOW_STACK_SIZE);
        AddLine("TEMP PBK[{}];", FLOW_STACK_SIZE);
        AddLine("TEMP SSY_TOP;");
        AddLine("TEMP PBK_TOP;");
    }

    AddLine("TEMP PC;");

    if (!ir.IsFlowStackDisabled()) {
        AddLine("MOV.U SSY_TOP.x, 0;");
        AddLine("MOV.U PBK_TOP.x, 0;");
    }

    InitializeVariables();

    const auto basic_block_end = ir.GetBasicBlocks().end();
    auto basic_block_it = ir.GetBasicBlocks().begin();
    const u32 first_address = basic_block_it->first;
    AddLine("MOV.U PC.x, {};", first_address);

    AddLine("REP;");

    std::size_t num_blocks = 0;
    while (basic_block_it != basic_block_end) {
        const auto& [address, bb] = *basic_block_it;
        ++num_blocks;

        AddLine("SEQ.S.CC RC.x, PC.x, {};", address);
        AddLine("IF NE.x;");

        VisitBlock(bb);

        ++basic_block_it;

        if (basic_block_it != basic_block_end) {
            const auto op = std::get_if<OperationNode>(&*bb[bb.size() - 1]);
            if (!op || op->GetCode() != OperationCode::Branch) {
                const u32 next_address = basic_block_it->first;
                AddLine("MOV.U PC.x, {};", next_address);
                AddLine("CONT;");
            }
        }

        AddLine("ELSE;");
    }
    AddLine("RET;");
    while (num_blocks--) {
        AddLine("ENDIF;");
    }

    AddLine("ENDREP;");
}

void ARBDecompiler::VisitAST(const ASTNode& node) {
    if (const auto ast = std::get_if<ASTProgram>(&*node->GetInnerData())) {
        for (ASTNode current = ast->nodes.GetFirst(); current; current = current->GetNext()) {
            VisitAST(current);
        }
    } else if (const auto ast = std::get_if<ASTIfThen>(&*node->GetInnerData())) {
        const std::string condition = VisitExpression(ast->condition);
        ResetTemporaries();

        AddLine("MOVC.U RC.x, {};", condition);
        AddLine("IF NE.x;");
        for (ASTNode current = ast->nodes.GetFirst(); current; current = current->GetNext()) {
            VisitAST(current);
        }
        AddLine("ENDIF;");
    } else if (const auto ast = std::get_if<ASTIfElse>(&*node->GetInnerData())) {
        AddLine("ELSE;");
        for (ASTNode current = ast->nodes.GetFirst(); current; current = current->GetNext()) {
            VisitAST(current);
        }
    } else if (const auto ast = std::get_if<ASTBlockDecoded>(&*node->GetInnerData())) {
        VisitBlock(ast->nodes);
    } else if (const auto ast = std::get_if<ASTVarSet>(&*node->GetInnerData())) {
        AddLine("MOV.U F{}, {};", ast->index, VisitExpression(ast->condition));
        ResetTemporaries();
    } else if (const auto ast = std::get_if<ASTDoWhile>(&*node->GetInnerData())) {
        const std::string condition = VisitExpression(ast->condition);
        ResetTemporaries();
        AddLine("REP;");
        for (ASTNode current = ast->nodes.GetFirst(); current; current = current->GetNext()) {
            VisitAST(current);
        }
        AddLine("MOVC.U RC.x, {};", condition);
        AddLine("BRK (NE.x);");
        AddLine("ENDREP;");
    } else if (const auto ast = std::get_if<ASTReturn>(&*node->GetInnerData())) {
        const bool is_true = ExprIsTrue(ast->condition);
        if (!is_true) {
            AddLine("MOVC.U RC.x, {};", VisitExpression(ast->condition));
            AddLine("IF NE.x;");
            ResetTemporaries();
        }
        if (ast->kills) {
            AddLine("KIL TR;");
        } else {
            Exit();
        }
        if (!is_true) {
            AddLine("ENDIF;");
        }
    } else if (const auto ast = std::get_if<ASTBreak>(&*node->GetInnerData())) {
        if (ExprIsTrue(ast->condition)) {
            AddLine("BRK;");
        } else {
            AddLine("MOVC.U RC.x, {};", VisitExpression(ast->condition));
            AddLine("BRK (NE.x);");
            ResetTemporaries();
        }
    } else if (std::holds_alternative<ASTLabel>(*node->GetInnerData())) {
        // Nothing to do
    } else {
        UNREACHABLE();
    }
}

std::string ARBDecompiler::VisitExpression(const Expr& node) {
    if (const auto expr = std::get_if<ExprAnd>(&*node)) {
        std::string result = AllocTemporary();
        AddLine("AND.U {}, {}, {};", result, VisitExpression(expr->operand1),
                VisitExpression(expr->operand2));
        return result;
    }
    if (const auto expr = std::get_if<ExprOr>(&*node)) {
        std::string result = AllocTemporary();
        AddLine("OR.U {}, {}, {};", result, VisitExpression(expr->operand1),
                VisitExpression(expr->operand2));
        return result;
    }
    if (const auto expr = std::get_if<ExprNot>(&*node)) {
        std::string result = AllocTemporary();
        AddLine("CMP.S {}, {}, 0, -1;", result, VisitExpression(expr->operand1));
        return result;
    }
    if (const auto expr = std::get_if<ExprPredicate>(&*node)) {
        return fmt::format("P{}.x", static_cast<u64>(expr->predicate));
    }
    if (const auto expr = std::get_if<ExprCondCode>(&*node)) {
        return Visit(ir.GetConditionCode(expr->cc));
    }
    if (const auto expr = std::get_if<ExprVar>(&*node)) {
        return fmt::format("F{}.x", expr->var_index);
    }
    if (const auto expr = std::get_if<ExprBoolean>(&*node)) {
        return expr->value ? "0xffffffff" : "0";
    }
    if (const auto expr = std::get_if<ExprGprEqual>(&*node)) {
        std::string result = AllocTemporary();
        AddLine("SEQ.U {}, R{}.x, {};", result, expr->gpr, expr->value);
        return result;
    }
    UNREACHABLE();
    return "0";
}

void ARBDecompiler::VisitBlock(const NodeBlock& bb) {
    for (const auto& node : bb) {
        Visit(node);
    }
}

std::string ARBDecompiler::Visit(const Node& node) {
    if (const auto operation = std::get_if<OperationNode>(&*node)) {
        if (const auto amend_index = operation->GetAmendIndex()) {
            Visit(ir.GetAmendNode(*amend_index));
        }
        const std::size_t index = static_cast<std::size_t>(operation->GetCode());
        if (index >= OPERATION_DECOMPILERS.size()) {
            UNREACHABLE_MSG("Out of bounds operation: {}", index);
            return {};
        }
        const auto decompiler = OPERATION_DECOMPILERS[index];
        if (decompiler == nullptr) {
            UNREACHABLE_MSG("Undefined operation: {}", index);
            return {};
        }
        return (this->*decompiler)(*operation);
    }

    if (const auto gpr = std::get_if<GprNode>(&*node)) {
        const u32 index = gpr->GetIndex();
        if (index == Register::ZeroIndex) {
            return "{0, 0, 0, 0}.x";
        }
        return fmt::format("R{}.x", index);
    }

    if (const auto cv = std::get_if<CustomVarNode>(&*node)) {
        return fmt::format("CV{}.x", cv->GetIndex());
    }

    if (const auto immediate = std::get_if<ImmediateNode>(&*node)) {
        std::string temporary = AllocTemporary();
        AddLine("MOV.U {}, {};", temporary, immediate->GetValue());
        return temporary;
    }

    if (const auto predicate = std::get_if<PredicateNode>(&*node)) {
        std::string temporary = AllocTemporary();
        switch (const auto index = predicate->GetIndex(); index) {
        case Tegra::Shader::Pred::UnusedIndex:
            AddLine("MOV.S {}, -1;", temporary);
            break;
        case Tegra::Shader::Pred::NeverExecute:
            AddLine("MOV.S {}, 0;", temporary);
            break;
        default:
            AddLine("MOV.S {}, P{}.x;", temporary, static_cast<u64>(index));
            break;
        }
        if (predicate->IsNegated()) {
            AddLine("CMP.S {}, {}, 0, -1;", temporary, temporary);
        }
        return temporary;
    }

    if (const auto abuf = std::get_if<AbufNode>(&*node)) {
        if (abuf->IsPhysicalBuffer()) {
            UNIMPLEMENTED_MSG("Physical buffers are not implemented");
            return "{0, 0, 0, 0}.x";
        }

        const Attribute::Index index = abuf->GetIndex();
        const u32 element = abuf->GetElement();
        const char swizzle = Swizzle(element);
        switch (index) {
        case Attribute::Index::Position: {
            if (stage == ShaderType::Geometry) {
                return fmt::format("{}_position[{}].{}", StageInputName(stage),
                                   Visit(abuf->GetBuffer()), swizzle);
            } else {
                return fmt::format("{}.position.{}", StageInputName(stage), swizzle);
            }
        }
        case Attribute::Index::TessCoordInstanceIDVertexID:
            ASSERT(stage == ShaderType::Vertex);
            switch (element) {
            case 2:
                return "vertex.instance";
            case 3:
                return "vertex.id";
            }
            UNIMPLEMENTED_MSG("Unmanaged TessCoordInstanceIDVertexID element={}", element);
            break;
        case Attribute::Index::PointCoord:
            switch (element) {
            case 0:
                return "fragment.pointcoord.x";
            case 1:
                return "fragment.pointcoord.y";
            }
            UNIMPLEMENTED();
            break;
        case Attribute::Index::FrontFacing: {
            ASSERT(stage == ShaderType::Fragment);
            ASSERT(element == 3);
            const std::string temporary = AllocVectorTemporary();
            AddLine("SGT.S RC.x, fragment.facing, {{0, 0, 0, 0}};");
            AddLine("MOV.U.CC RC.x, -RC;");
            AddLine("MOV.S {}.x, 0;", temporary);
            AddLine("MOV.S {}.x (NE.x), -1;", temporary);
            return fmt::format("{}.x", temporary);
        }
        default:
            if (IsGenericAttribute(index)) {
                if (stage == ShaderType::Geometry) {
                    return fmt::format("in_attr{}[{}][0].{}", GetGenericAttributeIndex(index),
                                       Visit(abuf->GetBuffer()), swizzle);
                } else {
                    return fmt::format("{}.attrib[{}].{}", StageInputName(stage),
                                       GetGenericAttributeIndex(index), swizzle);
                }
            }
            UNIMPLEMENTED_MSG("Unimplemented input attribute={}", static_cast<int>(index));
            break;
        }
        return "{0, 0, 0, 0}.x";
    }

    if (const auto cbuf = std::get_if<CbufNode>(&*node)) {
        std::string offset_string;
        const auto& offset = cbuf->GetOffset();
        if (const auto imm = std::get_if<ImmediateNode>(&*offset)) {
            offset_string = std::to_string(imm->GetValue());
        } else {
            offset_string = Visit(offset);
        }
        std::string temporary = AllocTemporary();
        AddLine("LDC.F32 {}, cbuf{}[{}];", temporary, cbuf->GetIndex(), offset_string);
        return temporary;
    }

    if (const auto gmem = std::get_if<GmemNode>(&*node)) {
        std::string temporary = AllocTemporary();
        AddLine("MOV {}, 0;", temporary);
        AddLine("LOAD.U32 {} (NE.x), {};", temporary, GlobalMemoryPointer(*gmem));
        return temporary;
    }

    if (const auto lmem = std::get_if<LmemNode>(&*node)) {
        std::string temporary = Visit(lmem->GetAddress());
        AddLine("SHR.U {}, {}, 2;", temporary, temporary);
        AddLine("MOV.U {}, lmem[{}].x;", temporary, temporary);
        return temporary;
    }

    if (const auto smem = std::get_if<SmemNode>(&*node)) {
        std::string temporary = Visit(smem->GetAddress());
        AddLine("LDS.U32 {}, shared_mem[{}];", temporary, temporary);
        return temporary;
    }

    if (const auto internal_flag = std::get_if<InternalFlagNode>(&*node)) {
        const std::size_t index = static_cast<std::size_t>(internal_flag->GetFlag());
        return fmt::format("{}.x", INTERNAL_FLAG_NAMES[index]);
    }

    if (const auto conditional = std::get_if<ConditionalNode>(&*node)) {
        if (const auto amend_index = conditional->GetAmendIndex()) {
            Visit(ir.GetAmendNode(*amend_index));
        }
        AddLine("MOVC.U RC.x, {};", Visit(conditional->GetCondition()));
        AddLine("IF NE.x;");
        VisitBlock(conditional->GetCode());
        AddLine("ENDIF;");
        return {};
    }

    if ([[maybe_unused]] const auto cmt = std::get_if<CommentNode>(&*node)) {
        // Uncommenting this will generate invalid code. GLASM lacks comments.
        // AddLine("// {}", cmt->GetText());
        return {};
    }

    UNIMPLEMENTED();
    return {};
}

std::tuple<std::string, std::string, std::size_t> ARBDecompiler::BuildCoords(Operation operation) {
    const auto& meta = std::get<MetaTexture>(operation.GetMeta());
    UNIMPLEMENTED_IF(meta.sampler.is_indexed);

    const bool is_extended = meta.sampler.is_shadow && meta.sampler.is_array &&
                             meta.sampler.type == Tegra::Shader::TextureType::TextureCube;
    const std::size_t count = operation.GetOperandsCount();
    std::string temporary = AllocVectorTemporary();
    std::size_t i = 0;
    for (; i < count; ++i) {
        AddLine("MOV.F {}.{}, {};", temporary, Swizzle(i), Visit(operation[i]));
    }
    if (meta.sampler.is_array) {
        AddLine("I2F.S {}.{}, {};", temporary, Swizzle(i), Visit(meta.array));
        ++i;
    }
    if (meta.sampler.is_shadow) {
        std::string compare = Visit(meta.depth_compare);
        if (is_extended) {
            ASSERT(i == 4);
            std::string extra_coord = AllocVectorTemporary();
            AddLine("MOV.F {}.x, {};", extra_coord, compare);
            return {fmt::format("{}, {}", temporary, extra_coord), extra_coord, 0};
        }
        AddLine("MOV.F {}.{}, {};", temporary, Swizzle(i), compare);
        ++i;
    }
    return {temporary, temporary, i};
}

std::string ARBDecompiler::BuildAoffi(Operation operation) {
    const auto& meta = std::get<MetaTexture>(operation.GetMeta());
    if (meta.aoffi.empty()) {
        return {};
    }
    const std::string temporary = AllocVectorTemporary();
    std::size_t i = 0;
    for (auto& node : meta.aoffi) {
        AddLine("MOV.S {}.{}, {};", temporary, Swizzle(i++), Visit(node));
    }
    return fmt::format(", offset({})", temporary);
}

std::string ARBDecompiler::GlobalMemoryPointer(const GmemNode& gmem) {
    // Read a bindless SSBO, return its address and set CC accordingly
    // address = c[binding].xy
    // length  = c[binding].z
    const u32 binding = global_memory_names.at(gmem.GetDescriptor());

    const std::string pointer = AllocLongVectorTemporary();
    std::string temporary = AllocTemporary();

    AddLine("PK64.U {}, c[{}];", pointer, binding);
    AddLine("SUB.U {}, {}, {};", temporary, Visit(gmem.GetRealAddress()),
            Visit(gmem.GetBaseAddress()));
    AddLine("CVT.U64.U32 {}.z, {};", pointer, temporary);
    AddLine("ADD.U64 {}.x, {}.x, {}.z;", pointer, pointer, pointer);
    // Compare offset to length and set CC
    AddLine("SLT.U.CC RC.x, {}, c[{}].z;", temporary, binding);
    return fmt::format("{}.x", pointer);
}

void ARBDecompiler::Exit() {
    if (stage != ShaderType::Fragment) {
        AddLine("RET;");
        return;
    }

    const auto safe_get_register = [this](u32 reg) -> std::string {
        const auto& used_registers = ir.GetRegisters();
        if (used_registers.contains(reg)) {
            return fmt::format("R{}.x", reg);
        }
        return "{0, 0, 0, 0}.x";
    };

    const auto& header = ir.GetHeader();
    u32 current_reg = 0;
    for (u32 rt = 0; rt < Tegra::Engines::Maxwell3D::Regs::NumRenderTargets; ++rt) {
        for (u32 component = 0; component < 4; ++component) {
            if (!header.ps.IsColorComponentOutputEnabled(rt, component)) {
                continue;
            }
            AddLine("MOV.F result_color{}.{}, {};", rt, Swizzle(component),
                    safe_get_register(current_reg));
            ++current_reg;
        }
    }
    if (header.ps.omap.depth) {
        AddLine("MOV.F result.depth.z, {};", safe_get_register(current_reg + 1));
    }

    AddLine("RET;");
}

std::string ARBDecompiler::Assign(Operation operation) {
    const Node& dest = operation[0];
    const Node& src = operation[1];

    std::string dest_name;
    if (const auto gpr = std::get_if<GprNode>(&*dest)) {
        if (gpr->GetIndex() == Register::ZeroIndex) {
            // Writing to Register::ZeroIndex is a no op
            return {};
        }
        dest_name = fmt::format("R{}.x", gpr->GetIndex());
    } else if (const auto abuf = std::get_if<AbufNode>(&*dest)) {
        const u32 element = abuf->GetElement();
        const char swizzle = Swizzle(element);
        switch (const Attribute::Index index = abuf->GetIndex()) {
        case Attribute::Index::Position:
            dest_name = fmt::format("result.position.{}", swizzle);
            break;
        case Attribute::Index::LayerViewportPointSize:
            switch (element) {
            case 0:
                UNIMPLEMENTED();
                return {};
            case 1:
            case 2:
                if (!device.HasNvViewportArray2()) {
                    LOG_ERROR(
                        Render_OpenGL,
                        "NV_viewport_array2 is missing. Maxwell gen 2 or better is required.");
                    return {};
                }
                dest_name = element == 1 ? "result.layer.x" : "result.viewport.x";
                break;
            case 3:
                dest_name = "result.pointsize.x";
                break;
            }
            break;
        case Attribute::Index::ClipDistances0123:
            dest_name = fmt::format("result.clip[{}].x", element);
            break;
        case Attribute::Index::ClipDistances4567:
            dest_name = fmt::format("result.clip[{}].x", element + 4);
            break;
        default:
            if (!IsGenericAttribute(index)) {
                UNREACHABLE();
                return {};
            }
            dest_name =
                fmt::format("result.attrib[{}].{}", GetGenericAttributeIndex(index), swizzle);
            break;
        }
    } else if (const auto lmem = std::get_if<LmemNode>(&*dest)) {
        const std::string address = Visit(lmem->GetAddress());
        AddLine("SHR.U {}, {}, 2;", address, address);
        dest_name = fmt::format("lmem[{}].x", address);
    } else if (const auto smem = std::get_if<SmemNode>(&*dest)) {
        AddLine("STS.U32 {}, shared_mem[{}];", Visit(src), Visit(smem->GetAddress()));
        ResetTemporaries();
        return {};
    } else if (const auto gmem = std::get_if<GmemNode>(&*dest)) {
        AddLine("IF NE.x;");
        AddLine("STORE.U32 {}, {};", Visit(src), GlobalMemoryPointer(*gmem));
        AddLine("ENDIF;");
        ResetTemporaries();
        return {};
    } else {
        UNREACHABLE();
        ResetTemporaries();
        return {};
    }

    AddLine("MOV.U {}, {};", dest_name, Visit(src));
    ResetTemporaries();
    return {};
}

std::string ARBDecompiler::Select(Operation operation) {
    std::string temporary = AllocTemporary();
    AddLine("CMP.S {}, {}, {}, {};", temporary, Visit(operation[0]), Visit(operation[1]),
            Visit(operation[2]));
    return temporary;
}

std::string ARBDecompiler::FClamp(Operation operation) {
    // 1.0f in hex, replace with std::bit_cast on C++20
    static constexpr u32 POSITIVE_ONE = 0x3f800000;

    std::string temporary = AllocTemporary();
    const Node& value = operation[0];
    const Node& low = operation[1];
    const Node& high = operation[2];
    const auto* const imm_low = std::get_if<ImmediateNode>(&*low);
    const auto* const imm_high = std::get_if<ImmediateNode>(&*high);
    if (imm_low && imm_high && imm_low->GetValue() == 0 && imm_high->GetValue() == POSITIVE_ONE) {
        AddLine("MOV.F32.SAT {}, {};", temporary, Visit(value));
    } else {
        AddLine("MIN.F {}, {}, {};", temporary, Visit(value), Visit(high));
        AddLine("MAX.F {}, {}, {};", temporary, temporary, Visit(low));
    }
    return temporary;
}

std::string ARBDecompiler::FCastHalf0(Operation operation) {
    const std::string temporary = AllocVectorTemporary();
    AddLine("UP2H.F {}.x, {};", temporary, Visit(operation[0]));
    return fmt::format("{}.x", temporary);
}

std::string ARBDecompiler::FCastHalf1(Operation operation) {
    const std::string temporary = AllocVectorTemporary();
    AddLine("UP2H.F {}.y, {};", temporary, Visit(operation[0]));
    AddLine("MOV {}.x, {}.y;", temporary, temporary);
    return fmt::format("{}.x", temporary);
}

std::string ARBDecompiler::FSqrt(Operation operation) {
    std::string temporary = AllocTemporary();
    AddLine("RSQ.F32 {}, {};", temporary, Visit(operation[0]));
    AddLine("RCP.F32 {}, {};", temporary, temporary);
    return temporary;
}

std::string ARBDecompiler::FSwizzleAdd(Operation operation) {
    const std::string temporary = AllocVectorTemporary();
    if (!device.HasWarpIntrinsics()) {
        LOG_ERROR(Render_OpenGL,
                  "NV_shader_thread_shuffle is missing. Kepler or better is required.");
        AddLine("ADD.F {}.x, {}, {};", temporary, Visit(operation[0]), Visit(operation[1]));
        return fmt::format("{}.x", temporary);
    }

    AddLine("AND.U {}.z, {}.threadid, 3;", temporary, StageInputName(stage));
    AddLine("SHL.U {}.z, {}.z, 1;", temporary, temporary);
    AddLine("SHR.U {}.z, {}, {}.z;", temporary, Visit(operation[2]), temporary);
    AddLine("AND.U {}.z, {}.z, 3;", temporary, temporary);
    AddLine("MUL.F32 {}.x, {}, FSWZA[{}.z];", temporary, Visit(operation[0]), temporary);
    AddLine("MUL.F32 {}.y, {}, FSWZB[{}.z];", temporary, Visit(operation[1]), temporary);
    AddLine("ADD.F32 {}.x, {}.x, {}.y;", temporary, temporary, temporary);
    return fmt::format("{}.x", temporary);
}

std::string ARBDecompiler::HAdd2(Operation operation) {
    const std::string tmp1 = AllocVectorTemporary();
    const std::string tmp2 = AllocVectorTemporary();
    AddLine("UP2H.F {}.xy, {};", tmp1, Visit(operation[0]));
    AddLine("UP2H.F {}.xy, {};", tmp2, Visit(operation[1]));
    AddLine("ADD.F16 {}, {}, {};", tmp1, tmp1, tmp2);
    AddLine("PK2H.F {}.x, {};", tmp1, tmp1);
    return fmt::format("{}.x", tmp1);
}

std::string ARBDecompiler::HMul2(Operation operation) {
    const std::string tmp1 = AllocVectorTemporary();
    const std::string tmp2 = AllocVectorTemporary();
    AddLine("UP2H.F {}.xy, {};", tmp1, Visit(operation[0]));
    AddLine("UP2H.F {}.xy, {};", tmp2, Visit(operation[1]));
    AddLine("MUL.F16 {}, {}, {};", tmp1, tmp1, tmp2);
    AddLine("PK2H.F {}.x, {};", tmp1, tmp1);
    return fmt::format("{}.x", tmp1);
}

std::string ARBDecompiler::HFma2(Operation operation) {
    const std::string tmp1 = AllocVectorTemporary();
    const std::string tmp2 = AllocVectorTemporary();
    const std::string tmp3 = AllocVectorTemporary();
    AddLine("UP2H.F {}.xy, {};", tmp1, Visit(operation[0]));
    AddLine("UP2H.F {}.xy, {};", tmp2, Visit(operation[1]));
    AddLine("UP2H.F {}.xy, {};", tmp3, Visit(operation[2]));
    AddLine("MAD.F16 {}, {}, {}, {};", tmp1, tmp1, tmp2, tmp3);
    AddLine("PK2H.F {}.x, {};", tmp1, tmp1);
    return fmt::format("{}.x", tmp1);
}

std::string ARBDecompiler::HAbsolute(Operation operation) {
    const std::string temporary = AllocVectorTemporary();
    AddLine("UP2H.F {}.xy, {};", temporary, Visit(operation[0]));
    AddLine("PK2H.F {}.x, |{}|;", temporary, temporary);
    return fmt::format("{}.x", temporary);
}

std::string ARBDecompiler::HNegate(Operation operation) {
    const std::string temporary = AllocVectorTemporary();
    AddLine("UP2H.F {}.xy, {};", temporary, Visit(operation[0]));
    AddLine("MOVC.S RC.x, {};", Visit(operation[1]));
    AddLine("MOV.F {}.x (NE.x), -{}.x;", temporary, temporary);
    AddLine("MOVC.S RC.x, {};", Visit(operation[2]));
    AddLine("MOV.F {}.y (NE.x), -{}.y;", temporary, temporary);
    AddLine("PK2H.F {}.x, {};", temporary, temporary);
    return fmt::format("{}.x", temporary);
}

std::string ARBDecompiler::HClamp(Operation operation) {
    const std::string tmp1 = AllocVectorTemporary();
    const std::string tmp2 = AllocVectorTemporary();
    AddLine("UP2H.F {}.xy, {};", tmp1, Visit(operation[0]));
    AddLine("MOV.U {}.x, {};", tmp2, Visit(operation[1]));
    AddLine("MOV.U {}.y, {}.x;", tmp2, tmp2);
    AddLine("MAX.F {}, {}, {};", tmp1, tmp1, tmp2);
    AddLine("MOV.U {}.x, {};", tmp2, Visit(operation[2]));
    AddLine("MOV.U {}.y, {}.x;", tmp2, tmp2);
    AddLine("MIN.F {}, {}, {};", tmp1, tmp1, tmp2);
    AddLine("PK2H.F {}.x, {};", tmp1, tmp1);
    return fmt::format("{}.x", tmp1);
}

std::string ARBDecompiler::HCastFloat(Operation operation) {
    const std::string temporary = AllocVectorTemporary();
    AddLine("MOV.F {}.y, {{0, 0, 0, 0}};", temporary);
    AddLine("MOV.F {}.x, {};", temporary, Visit(operation[0]));
    AddLine("PK2H.F {}.x, {};", temporary, temporary);
    return fmt::format("{}.x", temporary);
}

std::string ARBDecompiler::HUnpack(Operation operation) {
    std::string operand = Visit(operation[0]);
    switch (std::get<Tegra::Shader::HalfType>(operation.GetMeta())) {
    case Tegra::Shader::HalfType::H0_H1:
        return operand;
    case Tegra::Shader::HalfType::F32: {
        const std::string temporary = AllocVectorTemporary();
        AddLine("MOV.U {}.x, {};", temporary, operand);
        AddLine("MOV.U {}.y, {}.x;", temporary, temporary);
        AddLine("PK2H.F {}.x, {};", temporary, temporary);
        return fmt::format("{}.x", temporary);
    }
    case Tegra::Shader::HalfType::H0_H0: {
        const std::string temporary = AllocVectorTemporary();
        AddLine("UP2H.F {}.xy, {};", temporary, operand);
        AddLine("MOV.U {}.y, {}.x;", temporary, temporary);
        AddLine("PK2H.F {}.x, {};", temporary, temporary);
        return fmt::format("{}.x", temporary);
    }
    case Tegra::Shader::HalfType::H1_H1: {
        const std::string temporary = AllocVectorTemporary();
        AddLine("UP2H.F {}.xy, {};", temporary, operand);
        AddLine("MOV.U {}.x, {}.y;", temporary, temporary);
        AddLine("PK2H.F {}.x, {};", temporary, temporary);
        return fmt::format("{}.x", temporary);
    }
    }
    UNREACHABLE();
    return "{0, 0, 0, 0}.x";
}

std::string ARBDecompiler::HMergeF32(Operation operation) {
    const std::string temporary = AllocVectorTemporary();
    AddLine("UP2H.F {}.xy, {};", temporary, Visit(operation[0]));
    return fmt::format("{}.x", temporary);
}

std::string ARBDecompiler::HMergeH0(Operation operation) {
    const std::string temporary = AllocVectorTemporary();
    AddLine("UP2H.F {}.xy, {};", temporary, Visit(operation[0]));
    AddLine("UP2H.F {}.zw, {};", temporary, Visit(operation[1]));
    AddLine("MOV.U {}.x, {}.z;", temporary, temporary);
    AddLine("PK2H.F {}.x, {};", temporary, temporary);
    return fmt::format("{}.x", temporary);
}

std::string ARBDecompiler::HMergeH1(Operation operation) {
    const std::string temporary = AllocVectorTemporary();
    AddLine("UP2H.F {}.xy, {};", temporary, Visit(operation[0]));
    AddLine("UP2H.F {}.zw, {};", temporary, Visit(operation[1]));
    AddLine("MOV.U {}.y, {}.w;", temporary, temporary);
    AddLine("PK2H.F {}.x, {};", temporary, temporary);
    return fmt::format("{}.x", temporary);
}

std::string ARBDecompiler::HPack2(Operation operation) {
    const std::string temporary = AllocVectorTemporary();
    AddLine("MOV.U {}.x, {};", temporary, Visit(operation[0]));
    AddLine("MOV.U {}.y, {};", temporary, Visit(operation[1]));
    AddLine("PK2H.F {}.x, {};", temporary, temporary);
    return fmt::format("{}.x", temporary);
}

std::string ARBDecompiler::LogicalAssign(Operation operation) {
    const Node& dest = operation[0];
    const Node& src = operation[1];

    std::string target;

    if (const auto pred = std::get_if<PredicateNode>(&*dest)) {
        ASSERT_MSG(!pred->IsNegated(), "Negating logical assignment");

        const Tegra::Shader::Pred index = pred->GetIndex();
        switch (index) {
        case Tegra::Shader::Pred::NeverExecute:
        case Tegra::Shader::Pred::UnusedIndex:
            // Writing to these predicates is a no-op
            return {};
        }
        target = fmt::format("P{}.x", static_cast<u64>(index));
    } else if (const auto internal_flag = std::get_if<InternalFlagNode>(&*dest)) {
        const std::size_t index = static_cast<std::size_t>(internal_flag->GetFlag());
        target = fmt::format("{}.x", INTERNAL_FLAG_NAMES[index]);
    } else {
        UNREACHABLE();
        ResetTemporaries();
        return {};
    }

    AddLine("MOV.U {}, {};", target, Visit(src));
    ResetTemporaries();
    return {};
}

std::string ARBDecompiler::LogicalPick2(Operation operation) {
    std::string temporary = AllocTemporary();
    const u32 index = std::get<ImmediateNode>(*operation[1]).GetValue();
    AddLine("MOV.U {}, {}.{};", temporary, Visit(operation[0]), Swizzle(index));
    return temporary;
}

std::string ARBDecompiler::LogicalAnd2(Operation operation) {
    std::string temporary = AllocTemporary();
    const std::string op = Visit(operation[0]);
    AddLine("AND.U {}, {}.x, {}.y;", temporary, op, op);
    return temporary;
}

std::string ARBDecompiler::FloatOrdered(Operation operation) {
    std::string temporary = AllocTemporary();
    AddLine("MOVC.F32 RC.x, {};", Visit(operation[0]));
    AddLine("MOVC.F32 RC.y, {};", Visit(operation[1]));
    AddLine("MOV.S {}, -1;", temporary);
    AddLine("MOV.S {} (NAN.x), 0;", temporary);
    AddLine("MOV.S {} (NAN.y), 0;", temporary);
    return temporary;
}

std::string ARBDecompiler::FloatUnordered(Operation operation) {
    std::string temporary = AllocTemporary();
    AddLine("MOVC.F32 RC.x, {};", Visit(operation[0]));
    AddLine("MOVC.F32 RC.y, {};", Visit(operation[1]));
    AddLine("MOV.S {}, 0;", temporary);
    AddLine("MOV.S {} (NAN.x), -1;", temporary);
    AddLine("MOV.S {} (NAN.y), -1;", temporary);
    return temporary;
}

std::string ARBDecompiler::LogicalAddCarry(Operation operation) {
    std::string temporary = AllocTemporary();
    AddLine("ADDC.U RC, {}, {};", Visit(operation[0]), Visit(operation[1]));
    AddLine("MOV.S {}, 0;", temporary);
    AddLine("IF CF.x;");
    AddLine("MOV.S {}, -1;", temporary);
    AddLine("ENDIF;");
    return temporary;
}

std::string ARBDecompiler::Texture(Operation operation) {
    const auto& meta = std::get<MetaTexture>(operation.GetMeta());
    const u32 sampler_id = device.GetBaseBindings(stage).sampler + meta.sampler.index;
    const auto [coords, temporary, swizzle] = BuildCoords(operation);

    std::string_view opcode = "TEX";
    std::string extra;
    if (meta.bias) {
        ASSERT(!meta.lod);
        opcode = "TXB";

        if (swizzle < 4) {
            AddLine("MOV.F {}.w, {};", temporary, Visit(meta.bias));
        } else {
            const std::string bias = AllocTemporary();
            AddLine("MOV.F {}, {};", bias, Visit(meta.bias));
            extra = fmt::format(" {},", bias);
        }
    }
    if (meta.lod) {
        ASSERT(!meta.bias);
        opcode = "TXL";

        if (swizzle < 4) {
            AddLine("MOV.F {}.w, {};", temporary, Visit(meta.lod));
        } else {
            const std::string lod = AllocTemporary();
            AddLine("MOV.F {}, {};", lod, Visit(meta.lod));
            extra = fmt::format(" {},", lod);
        }
    }

    AddLine("{}.F {}, {},{} texture[{}], {}{};", opcode, temporary, coords, extra, sampler_id,
            TextureType(meta), BuildAoffi(operation));
    AddLine("MOV.U {}.x, {}.{};", temporary, temporary, Swizzle(meta.element));
    return fmt::format("{}.x", temporary);
}

std::string ARBDecompiler::TextureGather(Operation operation) {
    const auto& meta = std::get<MetaTexture>(operation.GetMeta());
    const u32 sampler_id = device.GetBaseBindings(stage).sampler + meta.sampler.index;
    const auto [coords, temporary, swizzle] = BuildCoords(operation);

    std::string comp;
    if (!meta.sampler.is_shadow) {
        const auto& immediate = std::get<ImmediateNode>(*meta.component);
        comp = fmt::format(".{}", Swizzle(immediate.GetValue()));
    }

    AddLine("TXG.F {}, {}, texture[{}]{}, {}{};", temporary, temporary, sampler_id, comp,
            TextureType(meta), BuildAoffi(operation));
    AddLine("MOV.U {}.x, {}.{};", temporary, coords, Swizzle(meta.element));
    return fmt::format("{}.x", temporary);
}

std::string ARBDecompiler::TextureQueryDimensions(Operation operation) {
    const auto& meta = std::get<MetaTexture>(operation.GetMeta());
    const std::string temporary = AllocVectorTemporary();
    const u32 sampler_id = device.GetBaseBindings(stage).sampler + meta.sampler.index;

    ASSERT(!meta.sampler.is_array);

    const std::string lod = operation.GetOperandsCount() > 0 ? Visit(operation[0]) : "0";
    AddLine("TXQ {}, {}, texture[{}], {};", temporary, lod, sampler_id, TextureType(meta));
    AddLine("MOV.U {}.x, {}.{};", temporary, temporary, Swizzle(meta.element));
    return fmt::format("{}.x", temporary);
}

std::string ARBDecompiler::TextureQueryLod(Operation operation) {
    const auto& meta = std::get<MetaTexture>(operation.GetMeta());
    const std::string temporary = AllocVectorTemporary();
    const u32 sampler_id = device.GetBaseBindings(stage).sampler + meta.sampler.index;

    ASSERT(!meta.sampler.is_array);

    const std::size_t count = operation.GetOperandsCount();
    for (std::size_t i = 0; i < count; ++i) {
        AddLine("MOV.F {}.{}, {};", temporary, Swizzle(i), Visit(operation[i]));
    }
    AddLine("LOD.F {}, {}, texture[{}], {};", temporary, temporary, sampler_id, TextureType(meta));
    AddLine("MUL.F32 {}, {}, {{256, 256, 0, 0}};", temporary, temporary);
    AddLine("TRUNC.S {}, {};", temporary, temporary);
    AddLine("MOV.U {}.x, {}.{};", temporary, temporary, Swizzle(meta.element));
    return fmt::format("{}.x", temporary);
}

std::string ARBDecompiler::TexelFetch(Operation operation) {
    const auto& meta = std::get<MetaTexture>(operation.GetMeta());
    const u32 sampler_id = device.GetBaseBindings(stage).sampler + meta.sampler.index;
    const auto [coords, temporary, swizzle] = BuildCoords(operation);

    if (!meta.sampler.is_buffer) {
        ASSERT(swizzle < 4);
        AddLine("MOV.F {}.w, {};", temporary, Visit(meta.lod));
    }
    AddLine("TXF.F {}, {}, texture[{}], {}{};", temporary, coords, sampler_id, TextureType(meta),
            BuildAoffi(operation));
    AddLine("MOV.U {}.x, {}.{};", temporary, temporary, Swizzle(meta.element));
    return fmt::format("{}.x", temporary);
}

std::string ARBDecompiler::TextureGradient(Operation operation) {
    const auto& meta = std::get<MetaTexture>(operation.GetMeta());
    const u32 sampler_id = device.GetBaseBindings(stage).sampler + meta.sampler.index;
    const std::string ddx = AllocVectorTemporary();
    const std::string ddy = AllocVectorTemporary();
    const std::string coord = std::get<1>(BuildCoords(operation));

    const std::size_t num_components = meta.derivates.size() / 2;
    for (std::size_t index = 0; index < num_components; ++index) {
        const char swizzle = Swizzle(index);
        AddLine("MOV.F {}.{}, {};", ddx, swizzle, Visit(meta.derivates[index * 2]));
        AddLine("MOV.F {}.{}, {};", ddy, swizzle, Visit(meta.derivates[index * 2 + 1]));
    }

    const std::string_view result = coord;
    AddLine("TXD.F {}, {}, {}, {}, texture[{}], {}{};", result, coord, ddx, ddy, sampler_id,
            TextureType(meta), BuildAoffi(operation));
    AddLine("MOV.F {}.x, {}.{};", result, result, Swizzle(meta.element));
    return fmt::format("{}.x", result);
}

std::string ARBDecompiler::ImageLoad(Operation operation) {
    const auto& meta = std::get<MetaImage>(operation.GetMeta());
    const u32 image_id = device.GetBaseBindings(stage).image + meta.image.index;
    const std::size_t count = operation.GetOperandsCount();
    const std::string_view type = ImageType(meta.image.type);

    const std::string temporary = AllocVectorTemporary();
    for (std::size_t i = 0; i < count; ++i) {
        AddLine("MOV.S {}.{}, {};", temporary, Swizzle(i), Visit(operation[i]));
    }
    AddLine("LOADIM.F {}, {}, image[{}], {};", temporary, temporary, image_id, type);
    AddLine("MOV.F {}.x, {}.{};", temporary, temporary, Swizzle(meta.element));
    return fmt::format("{}.x", temporary);
}

std::string ARBDecompiler::ImageStore(Operation operation) {
    const auto& meta = std::get<MetaImage>(operation.GetMeta());
    const u32 image_id = device.GetBaseBindings(stage).image + meta.image.index;
    const std::size_t num_coords = operation.GetOperandsCount();
    const std::size_t num_values = meta.values.size();
    const std::string_view type = ImageType(meta.image.type);

    const std::string coord = AllocVectorTemporary();
    const std::string value = AllocVectorTemporary();
    for (std::size_t i = 0; i < num_coords; ++i) {
        AddLine("MOV.S {}.{}, {};", coord, Swizzle(i), Visit(operation[i]));
    }
    for (std::size_t i = 0; i < num_values; ++i) {
        AddLine("MOV.F {}.{}, {};", value, Swizzle(i), Visit(meta.values[i]));
    }
    AddLine("STOREIM.F image[{}], {}, {}, {};", image_id, value, coord, type);
    return {};
}

std::string ARBDecompiler::Branch(Operation operation) {
    const auto target = std::get<ImmediateNode>(*operation[0]);
    AddLine("MOV.U PC.x, {};", target.GetValue());
    AddLine("CONT;");
    return {};
}

std::string ARBDecompiler::BranchIndirect(Operation operation) {
    AddLine("MOV.U PC.x, {};", Visit(operation[0]));
    AddLine("CONT;");
    return {};
}

std::string ARBDecompiler::PushFlowStack(Operation operation) {
    const auto stack = std::get<MetaStackClass>(operation.GetMeta());
    const u32 target = std::get<ImmediateNode>(*operation[0]).GetValue();
    const std::string_view stack_name = StackName(stack);
    AddLine("MOV.U {}[{}_TOP.x].x, {};", stack_name, stack_name, target);
    AddLine("ADD.S {}_TOP.x, {}_TOP.x, 1;", stack_name, stack_name);
    return {};
}

std::string ARBDecompiler::PopFlowStack(Operation operation) {
    const auto stack = std::get<MetaStackClass>(operation.GetMeta());
    const std::string_view stack_name = StackName(stack);
    AddLine("SUB.S {}_TOP.x, {}_TOP.x, 1;", stack_name, stack_name);
    AddLine("MOV.U PC.x, {}[{}_TOP.x].x;", stack_name, stack_name);
    AddLine("CONT;");
    return {};
}

std::string ARBDecompiler::Exit(Operation) {
    Exit();
    return {};
}

std::string ARBDecompiler::Discard(Operation) {
    AddLine("KIL TR;");
    return {};
}

std::string ARBDecompiler::EmitVertex(Operation) {
    AddLine("EMIT;");
    return {};
}

std::string ARBDecompiler::EndPrimitive(Operation) {
    AddLine("ENDPRIM;");
    return {};
}

std::string ARBDecompiler::InvocationId(Operation) {
    return "primitive.invocation";
}

std::string ARBDecompiler::YNegate(Operation) {
    LOG_WARNING(Render_OpenGL, "(STUBBED)");
    std::string temporary = AllocTemporary();
    AddLine("MOV.F {}, 1;", temporary);
    return temporary;
}

std::string ARBDecompiler::ThreadId(Operation) {
    return fmt::format("{}.threadid", StageInputName(stage));
}

std::string ARBDecompiler::ShuffleIndexed(Operation operation) {
    if (!device.HasWarpIntrinsics()) {
        LOG_ERROR(Render_OpenGL,
                  "NV_shader_thread_shuffle is missing. Kepler or better is required.");
        return Visit(operation[0]);
    }
    const std::string temporary = AllocVectorTemporary();
    AddLine("SHFIDX.U {}, {}, {}, {{31, 0, 0, 0}};", temporary, Visit(operation[0]),
            Visit(operation[1]));
    AddLine("MOV.U {}.x, {}.y;", temporary, temporary);
    return fmt::format("{}.x", temporary);
}

std::string ARBDecompiler::Barrier(Operation) {
    AddLine("BAR;");
    return {};
}

std::string ARBDecompiler::MemoryBarrierGroup(Operation) {
    AddLine("MEMBAR.CTA;");
    return {};
}

std::string ARBDecompiler::MemoryBarrierGlobal(Operation) {
    AddLine("MEMBAR;");
    return {};
}

} // Anonymous namespace

std::string DecompileAssemblyShader(const Device& device, const VideoCommon::Shader::ShaderIR& ir,
                                    const VideoCommon::Shader::Registry& registry,
                                    Tegra::Engines::ShaderType stage, std::string_view identifier) {
    return ARBDecompiler(device, ir, registry, stage, identifier).Code();
}

} // namespace OpenGL
