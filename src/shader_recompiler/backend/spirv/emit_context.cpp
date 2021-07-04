// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <climits>
#include <string_view>

#include <fmt/format.h>

#include "common/common_types.h"
#include "common/div_ceil.h"
#include "shader_recompiler/backend/spirv/emit_context.h"

namespace Shader::Backend::SPIRV {
namespace {
enum class Operation {
    Increment,
    Decrement,
    FPAdd,
    FPMin,
    FPMax,
};

struct AttrInfo {
    Id pointer;
    Id id;
    bool needs_cast;
};

Id ImageType(EmitContext& ctx, const TextureDescriptor& desc) {
    const spv::ImageFormat format{spv::ImageFormat::Unknown};
    const Id type{ctx.F32[1]};
    const bool depth{desc.is_depth};
    switch (desc.type) {
    case TextureType::Color1D:
        return ctx.TypeImage(type, spv::Dim::Dim1D, depth, false, false, 1, format);
    case TextureType::ColorArray1D:
        return ctx.TypeImage(type, spv::Dim::Dim1D, depth, true, false, 1, format);
    case TextureType::Color2D:
        return ctx.TypeImage(type, spv::Dim::Dim2D, depth, false, false, 1, format);
    case TextureType::ColorArray2D:
        return ctx.TypeImage(type, spv::Dim::Dim2D, depth, true, false, 1, format);
    case TextureType::Color3D:
        return ctx.TypeImage(type, spv::Dim::Dim3D, depth, false, false, 1, format);
    case TextureType::ColorCube:
        return ctx.TypeImage(type, spv::Dim::Cube, depth, false, false, 1, format);
    case TextureType::ColorArrayCube:
        return ctx.TypeImage(type, spv::Dim::Cube, depth, true, false, 1, format);
    case TextureType::Buffer:
        break;
    }
    throw InvalidArgument("Invalid texture type {}", desc.type);
}

spv::ImageFormat GetImageFormat(ImageFormat format) {
    switch (format) {
    case ImageFormat::Typeless:
        return spv::ImageFormat::Unknown;
    case ImageFormat::R8_UINT:
        return spv::ImageFormat::R8ui;
    case ImageFormat::R8_SINT:
        return spv::ImageFormat::R8i;
    case ImageFormat::R16_UINT:
        return spv::ImageFormat::R16ui;
    case ImageFormat::R16_SINT:
        return spv::ImageFormat::R16i;
    case ImageFormat::R32_UINT:
        return spv::ImageFormat::R32ui;
    case ImageFormat::R32G32_UINT:
        return spv::ImageFormat::Rg32ui;
    case ImageFormat::R32G32B32A32_UINT:
        return spv::ImageFormat::Rgba32ui;
    }
    throw InvalidArgument("Invalid image format {}", format);
}

Id ImageType(EmitContext& ctx, const ImageDescriptor& desc) {
    const spv::ImageFormat format{GetImageFormat(desc.format)};
    const Id type{ctx.U32[1]};
    switch (desc.type) {
    case TextureType::Color1D:
        return ctx.TypeImage(type, spv::Dim::Dim1D, false, false, false, 2, format);
    case TextureType::ColorArray1D:
        return ctx.TypeImage(type, spv::Dim::Dim1D, false, true, false, 2, format);
    case TextureType::Color2D:
        return ctx.TypeImage(type, spv::Dim::Dim2D, false, false, false, 2, format);
    case TextureType::ColorArray2D:
        return ctx.TypeImage(type, spv::Dim::Dim2D, false, true, false, 2, format);
    case TextureType::Color3D:
        return ctx.TypeImage(type, spv::Dim::Dim3D, false, false, false, 2, format);
    case TextureType::Buffer:
        throw NotImplementedException("Image buffer");
    default:
        break;
    }
    throw InvalidArgument("Invalid texture type {}", desc.type);
}

Id DefineVariable(EmitContext& ctx, Id type, std::optional<spv::BuiltIn> builtin,
                  spv::StorageClass storage_class) {
    const Id pointer_type{ctx.TypePointer(storage_class, type)};
    const Id id{ctx.AddGlobalVariable(pointer_type, storage_class)};
    if (builtin) {
        ctx.Decorate(id, spv::Decoration::BuiltIn, *builtin);
    }
    ctx.interfaces.push_back(id);
    return id;
}

u32 NumVertices(InputTopology input_topology) {
    switch (input_topology) {
    case InputTopology::Points:
        return 1;
    case InputTopology::Lines:
        return 2;
    case InputTopology::LinesAdjacency:
        return 4;
    case InputTopology::Triangles:
        return 3;
    case InputTopology::TrianglesAdjacency:
        return 6;
    }
    throw InvalidArgument("Invalid input topology {}", input_topology);
}

Id DefineInput(EmitContext& ctx, Id type, bool per_invocation,
               std::optional<spv::BuiltIn> builtin = std::nullopt) {
    switch (ctx.stage) {
    case Stage::TessellationControl:
    case Stage::TessellationEval:
        if (per_invocation) {
            type = ctx.TypeArray(type, ctx.Const(32u));
        }
        break;
    case Stage::Geometry:
        if (per_invocation) {
            const u32 num_vertices{NumVertices(ctx.runtime_info.input_topology)};
            type = ctx.TypeArray(type, ctx.Const(num_vertices));
        }
        break;
    default:
        break;
    }
    return DefineVariable(ctx, type, builtin, spv::StorageClass::Input);
}

Id DefineOutput(EmitContext& ctx, Id type, std::optional<u32> invocations,
                std::optional<spv::BuiltIn> builtin = std::nullopt) {
    if (invocations && ctx.stage == Stage::TessellationControl) {
        type = ctx.TypeArray(type, ctx.Const(*invocations));
    }
    return DefineVariable(ctx, type, builtin, spv::StorageClass::Output);
}

void DefineGenericOutput(EmitContext& ctx, size_t index, std::optional<u32> invocations) {
    static constexpr std::string_view swizzle{"xyzw"};
    const size_t base_attr_index{static_cast<size_t>(IR::Attribute::Generic0X) + index * 4};
    u32 element{0};
    while (element < 4) {
        const u32 remainder{4 - element};
        const TransformFeedbackVarying* xfb_varying{};
        if (!ctx.runtime_info.xfb_varyings.empty()) {
            xfb_varying = &ctx.runtime_info.xfb_varyings[base_attr_index + element];
            xfb_varying = xfb_varying && xfb_varying->components > 0 ? xfb_varying : nullptr;
        }
        const u32 num_components{xfb_varying ? xfb_varying->components : remainder};

        const Id id{DefineOutput(ctx, ctx.F32[num_components], invocations)};
        ctx.Decorate(id, spv::Decoration::Location, static_cast<u32>(index));
        if (element > 0) {
            ctx.Decorate(id, spv::Decoration::Component, element);
        }
        if (xfb_varying) {
            ctx.Decorate(id, spv::Decoration::XfbBuffer, xfb_varying->buffer);
            ctx.Decorate(id, spv::Decoration::XfbStride, xfb_varying->stride);
            ctx.Decorate(id, spv::Decoration::Offset, xfb_varying->offset);
        }
        if (num_components < 4 || element > 0) {
            const std::string_view subswizzle{swizzle.substr(element, num_components)};
            ctx.Name(id, fmt::format("out_attr{}_{}", index, subswizzle));
        } else {
            ctx.Name(id, fmt::format("out_attr{}", index));
        }
        const GenericElementInfo info{
            .id = id,
            .first_element = element,
            .num_components = num_components,
        };
        std::fill_n(ctx.output_generics[index].begin() + element, num_components, info);
        element += num_components;
    }
}

Id GetAttributeType(EmitContext& ctx, AttributeType type) {
    switch (type) {
    case AttributeType::Float:
        return ctx.F32[4];
    case AttributeType::SignedInt:
        return ctx.TypeVector(ctx.TypeInt(32, true), 4);
    case AttributeType::UnsignedInt:
        return ctx.U32[4];
    case AttributeType::Disabled:
        break;
    }
    throw InvalidArgument("Invalid attribute type {}", type);
}

std::optional<AttrInfo> AttrTypes(EmitContext& ctx, u32 index) {
    const AttributeType type{ctx.runtime_info.generic_input_types.at(index)};
    switch (type) {
    case AttributeType::Float:
        return AttrInfo{ctx.input_f32, ctx.F32[1], false};
    case AttributeType::UnsignedInt:
        return AttrInfo{ctx.input_u32, ctx.U32[1], true};
    case AttributeType::SignedInt:
        return AttrInfo{ctx.input_s32, ctx.TypeInt(32, true), true};
    case AttributeType::Disabled:
        return std::nullopt;
    }
    throw InvalidArgument("Invalid attribute type {}", type);
}

std::string_view StageName(Stage stage) {
    switch (stage) {
    case Stage::VertexA:
        return "vs_a";
    case Stage::VertexB:
        return "vs";
    case Stage::TessellationControl:
        return "tcs";
    case Stage::TessellationEval:
        return "tes";
    case Stage::Geometry:
        return "gs";
    case Stage::Fragment:
        return "fs";
    case Stage::Compute:
        return "cs";
    }
    throw InvalidArgument("Invalid stage {}", stage);
}

template <typename... Args>
void Name(EmitContext& ctx, Id object, std::string_view format_str, Args&&... args) {
    ctx.Name(object, fmt::format(fmt::runtime(format_str), StageName(ctx.stage),
                                 std::forward<Args>(args)...)
                         .c_str());
}

void DefineConstBuffers(EmitContext& ctx, const Info& info, Id UniformDefinitions::*member_type,
                        u32 binding, Id type, char type_char, u32 element_size) {
    const Id array_type{ctx.TypeArray(type, ctx.Const(65536U / element_size))};
    ctx.Decorate(array_type, spv::Decoration::ArrayStride, element_size);

    const Id struct_type{ctx.TypeStruct(array_type)};
    Name(ctx, struct_type, "{}_cbuf_block_{}{}", ctx.stage, type_char, element_size * CHAR_BIT);
    ctx.Decorate(struct_type, spv::Decoration::Block);
    ctx.MemberName(struct_type, 0, "data");
    ctx.MemberDecorate(struct_type, 0, spv::Decoration::Offset, 0U);

    const Id struct_pointer_type{ctx.TypePointer(spv::StorageClass::Uniform, struct_type)};
    const Id uniform_type{ctx.TypePointer(spv::StorageClass::Uniform, type)};
    ctx.uniform_types.*member_type = uniform_type;

    for (const ConstantBufferDescriptor& desc : info.constant_buffer_descriptors) {
        const Id id{ctx.AddGlobalVariable(struct_pointer_type, spv::StorageClass::Uniform)};
        ctx.Decorate(id, spv::Decoration::Binding, binding);
        ctx.Decorate(id, spv::Decoration::DescriptorSet, 0U);
        ctx.Name(id, fmt::format("c{}", desc.index));
        for (size_t i = 0; i < desc.count; ++i) {
            ctx.cbufs[desc.index + i].*member_type = id;
        }
        if (ctx.profile.supported_spirv >= 0x00010400) {
            ctx.interfaces.push_back(id);
        }
        binding += desc.count;
    }
}

void DefineSsbos(EmitContext& ctx, StorageTypeDefinition& type_def,
                 Id StorageDefinitions::*member_type, const Info& info, u32 binding, Id type,
                 u32 stride) {
    const Id array_type{ctx.TypeRuntimeArray(type)};
    ctx.Decorate(array_type, spv::Decoration::ArrayStride, stride);

    const Id struct_type{ctx.TypeStruct(array_type)};
    ctx.Decorate(struct_type, spv::Decoration::Block);
    ctx.MemberDecorate(struct_type, 0, spv::Decoration::Offset, 0U);

    const Id struct_pointer{ctx.TypePointer(spv::StorageClass::StorageBuffer, struct_type)};
    type_def.array = struct_pointer;
    type_def.element = ctx.TypePointer(spv::StorageClass::StorageBuffer, type);

    u32 index{};
    for (const StorageBufferDescriptor& desc : info.storage_buffers_descriptors) {
        const Id id{ctx.AddGlobalVariable(struct_pointer, spv::StorageClass::StorageBuffer)};
        ctx.Decorate(id, spv::Decoration::Binding, binding);
        ctx.Decorate(id, spv::Decoration::DescriptorSet, 0U);
        ctx.Name(id, fmt::format("ssbo{}", index));
        if (ctx.profile.supported_spirv >= 0x00010400) {
            ctx.interfaces.push_back(id);
        }
        for (size_t i = 0; i < desc.count; ++i) {
            ctx.ssbos[index + i].*member_type = id;
        }
        index += desc.count;
        binding += desc.count;
    }
}

Id CasFunction(EmitContext& ctx, Operation operation, Id value_type) {
    const Id func_type{ctx.TypeFunction(value_type, value_type, value_type)};
    const Id func{ctx.OpFunction(value_type, spv::FunctionControlMask::MaskNone, func_type)};
    const Id op_a{ctx.OpFunctionParameter(value_type)};
    const Id op_b{ctx.OpFunctionParameter(value_type)};
    ctx.AddLabel();
    Id result{};
    switch (operation) {
    case Operation::Increment: {
        const Id pred{ctx.OpUGreaterThanEqual(ctx.U1, op_a, op_b)};
        const Id incr{ctx.OpIAdd(value_type, op_a, ctx.Constant(value_type, 1))};
        result = ctx.OpSelect(value_type, pred, ctx.u32_zero_value, incr);
        break;
    }
    case Operation::Decrement: {
        const Id lhs{ctx.OpIEqual(ctx.U1, op_a, ctx.Constant(value_type, 0u))};
        const Id rhs{ctx.OpUGreaterThan(ctx.U1, op_a, op_b)};
        const Id pred{ctx.OpLogicalOr(ctx.U1, lhs, rhs)};
        const Id decr{ctx.OpISub(value_type, op_a, ctx.Constant(value_type, 1))};
        result = ctx.OpSelect(value_type, pred, op_b, decr);
        break;
    }
    case Operation::FPAdd:
        result = ctx.OpFAdd(value_type, op_a, op_b);
        break;
    case Operation::FPMin:
        result = ctx.OpFMin(value_type, op_a, op_b);
        break;
    case Operation::FPMax:
        result = ctx.OpFMax(value_type, op_a, op_b);
        break;
    default:
        break;
    }
    ctx.OpReturnValue(result);
    ctx.OpFunctionEnd();
    return func;
}

Id CasLoop(EmitContext& ctx, Operation operation, Id array_pointer, Id element_pointer,
           Id value_type, Id memory_type, spv::Scope scope) {
    const bool is_shared{scope == spv::Scope::Workgroup};
    const bool is_struct{!is_shared || ctx.profile.support_explicit_workgroup_layout};
    const Id cas_func{CasFunction(ctx, operation, value_type)};
    const Id zero{ctx.u32_zero_value};
    const Id scope_id{ctx.Const(static_cast<u32>(scope))};

    const Id loop_header{ctx.OpLabel()};
    const Id continue_block{ctx.OpLabel()};
    const Id merge_block{ctx.OpLabel()};
    const Id func_type{is_shared
                           ? ctx.TypeFunction(value_type, ctx.U32[1], value_type)
                           : ctx.TypeFunction(value_type, ctx.U32[1], value_type, array_pointer)};

    const Id func{ctx.OpFunction(value_type, spv::FunctionControlMask::MaskNone, func_type)};
    const Id index{ctx.OpFunctionParameter(ctx.U32[1])};
    const Id op_b{ctx.OpFunctionParameter(value_type)};
    const Id base{is_shared ? ctx.shared_memory_u32 : ctx.OpFunctionParameter(array_pointer)};
    ctx.AddLabel();
    ctx.OpBranch(loop_header);
    ctx.AddLabel(loop_header);

    ctx.OpLoopMerge(merge_block, continue_block, spv::LoopControlMask::MaskNone);
    ctx.OpBranch(continue_block);

    ctx.AddLabel(continue_block);
    const Id word_pointer{is_struct ? ctx.OpAccessChain(element_pointer, base, zero, index)
                                    : ctx.OpAccessChain(element_pointer, base, index)};
    if (value_type.value == ctx.F32[2].value) {
        const Id u32_value{ctx.OpLoad(ctx.U32[1], word_pointer)};
        const Id value{ctx.OpUnpackHalf2x16(ctx.F32[2], u32_value)};
        const Id new_value{ctx.OpFunctionCall(value_type, cas_func, value, op_b)};
        const Id u32_new_value{ctx.OpPackHalf2x16(ctx.U32[1], new_value)};
        const Id atomic_res{ctx.OpAtomicCompareExchange(ctx.U32[1], word_pointer, scope_id, zero,
                                                        zero, u32_new_value, u32_value)};
        const Id success{ctx.OpIEqual(ctx.U1, atomic_res, u32_value)};
        ctx.OpBranchConditional(success, merge_block, loop_header);

        ctx.AddLabel(merge_block);
        ctx.OpReturnValue(ctx.OpUnpackHalf2x16(ctx.F32[2], atomic_res));
    } else {
        const Id value{ctx.OpLoad(memory_type, word_pointer)};
        const bool matching_type{value_type.value == memory_type.value};
        const Id bitcast_value{matching_type ? value : ctx.OpBitcast(value_type, value)};
        const Id cal_res{ctx.OpFunctionCall(value_type, cas_func, bitcast_value, op_b)};
        const Id new_value{matching_type ? cal_res : ctx.OpBitcast(memory_type, cal_res)};
        const Id atomic_res{ctx.OpAtomicCompareExchange(ctx.U32[1], word_pointer, scope_id, zero,
                                                        zero, new_value, value)};
        const Id success{ctx.OpIEqual(ctx.U1, atomic_res, value)};
        ctx.OpBranchConditional(success, merge_block, loop_header);

        ctx.AddLabel(merge_block);
        ctx.OpReturnValue(ctx.OpBitcast(value_type, atomic_res));
    }
    ctx.OpFunctionEnd();
    return func;
}

template <typename Desc>
std::string NameOf(Stage stage, const Desc& desc, std::string_view prefix) {
    if (desc.count > 1) {
        return fmt::format("{}_{}{}_{:02x}x{}", StageName(stage), prefix, desc.cbuf_index,
                           desc.cbuf_offset, desc.count);
    } else {
        return fmt::format("{}_{}{}_{:02x}", StageName(stage), prefix, desc.cbuf_index,
                           desc.cbuf_offset);
    }
}

Id DescType(EmitContext& ctx, Id sampled_type, Id pointer_type, u32 count) {
    if (count > 1) {
        const Id array_type{ctx.TypeArray(sampled_type, ctx.Const(count))};
        return ctx.TypePointer(spv::StorageClass::UniformConstant, array_type);
    } else {
        return pointer_type;
    }
}
} // Anonymous namespace

void VectorTypes::Define(Sirit::Module& sirit_ctx, Id base_type, std::string_view name) {
    defs[0] = sirit_ctx.Name(base_type, name);

    std::array<char, 6> def_name;
    for (int i = 1; i < 4; ++i) {
        const std::string_view def_name_view(
            def_name.data(),
            fmt::format_to_n(def_name.data(), def_name.size(), "{}x{}", name, i + 1).size);
        defs[static_cast<size_t>(i)] =
            sirit_ctx.Name(sirit_ctx.TypeVector(base_type, i + 1), def_name_view);
    }
}

EmitContext::EmitContext(const Profile& profile_, const RuntimeInfo& runtime_info_,
                         IR::Program& program, Bindings& bindings)
    : Sirit::Module(profile_.supported_spirv), profile{profile_},
      runtime_info{runtime_info_}, stage{program.stage} {
    const bool is_unified{profile.unified_descriptor_binding};
    u32& uniform_binding{is_unified ? bindings.unified : bindings.uniform_buffer};
    u32& storage_binding{is_unified ? bindings.unified : bindings.storage_buffer};
    u32& texture_binding{is_unified ? bindings.unified : bindings.texture};
    u32& image_binding{is_unified ? bindings.unified : bindings.image};
    AddCapability(spv::Capability::Shader);
    DefineCommonTypes(program.info);
    DefineCommonConstants();
    DefineInterfaces(program);
    DefineLocalMemory(program);
    DefineSharedMemory(program);
    DefineSharedMemoryFunctions(program);
    DefineConstantBuffers(program.info, uniform_binding);
    DefineStorageBuffers(program.info, storage_binding);
    DefineTextureBuffers(program.info, texture_binding);
    DefineImageBuffers(program.info, image_binding);
    DefineTextures(program.info, texture_binding);
    DefineImages(program.info, image_binding);
    DefineAttributeMemAccess(program.info);
    DefineGlobalMemoryFunctions(program.info);
}

EmitContext::~EmitContext() = default;

Id EmitContext::Def(const IR::Value& value) {
    if (!value.IsImmediate()) {
        return value.InstRecursive()->Definition<Id>();
    }
    switch (value.Type()) {
    case IR::Type::Void:
        // Void instructions are used for optional arguments (e.g. texture offsets)
        // They are not meant to be used in the SPIR-V module
        return Id{};
    case IR::Type::U1:
        return value.U1() ? true_value : false_value;
    case IR::Type::U32:
        return Const(value.U32());
    case IR::Type::U64:
        return Constant(U64, value.U64());
    case IR::Type::F32:
        return Const(value.F32());
    case IR::Type::F64:
        return Constant(F64[1], value.F64());
    default:
        throw NotImplementedException("Immediate type {}", value.Type());
    }
}

Id EmitContext::BitOffset8(const IR::Value& offset) {
    if (offset.IsImmediate()) {
        return Const((offset.U32() % 4) * 8);
    }
    return OpBitwiseAnd(U32[1], OpShiftLeftLogical(U32[1], Def(offset), Const(3u)), Const(24u));
}

Id EmitContext::BitOffset16(const IR::Value& offset) {
    if (offset.IsImmediate()) {
        return Const(((offset.U32() / 2) % 2) * 16);
    }
    return OpBitwiseAnd(U32[1], OpShiftLeftLogical(U32[1], Def(offset), Const(3u)), Const(16u));
}

void EmitContext::DefineCommonTypes(const Info& info) {
    void_id = TypeVoid();

    U1 = Name(TypeBool(), "u1");

    F32.Define(*this, TypeFloat(32), "f32");
    U32.Define(*this, TypeInt(32, false), "u32");
    S32.Define(*this, TypeInt(32, true), "s32");

    private_u32 = Name(TypePointer(spv::StorageClass::Private, U32[1]), "private_u32");

    input_f32 = Name(TypePointer(spv::StorageClass::Input, F32[1]), "input_f32");
    input_u32 = Name(TypePointer(spv::StorageClass::Input, U32[1]), "input_u32");
    input_s32 = Name(TypePointer(spv::StorageClass::Input, TypeInt(32, true)), "input_s32");

    output_f32 = Name(TypePointer(spv::StorageClass::Output, F32[1]), "output_f32");
    output_u32 = Name(TypePointer(spv::StorageClass::Output, U32[1]), "output_u32");

    if (info.uses_int8 && profile.support_int8) {
        AddCapability(spv::Capability::Int8);
        U8 = Name(TypeInt(8, false), "u8");
        S8 = Name(TypeInt(8, true), "s8");
    }
    if (info.uses_int16 && profile.support_int16) {
        AddCapability(spv::Capability::Int16);
        U16 = Name(TypeInt(16, false), "u16");
        S16 = Name(TypeInt(16, true), "s16");
    }
    if (info.uses_int64) {
        AddCapability(spv::Capability::Int64);
        U64 = Name(TypeInt(64, false), "u64");
    }
    if (info.uses_fp16) {
        AddCapability(spv::Capability::Float16);
        F16.Define(*this, TypeFloat(16), "f16");
    }
    if (info.uses_fp64) {
        AddCapability(spv::Capability::Float64);
        F64.Define(*this, TypeFloat(64), "f64");
    }
}

void EmitContext::DefineCommonConstants() {
    true_value = ConstantTrue(U1);
    false_value = ConstantFalse(U1);
    u32_zero_value = Const(0U);
    f32_zero_value = Const(0.0f);
}

void EmitContext::DefineInterfaces(const IR::Program& program) {
    DefineInputs(program);
    DefineOutputs(program);
}

void EmitContext::DefineLocalMemory(const IR::Program& program) {
    if (program.local_memory_size == 0) {
        return;
    }
    const u32 num_elements{Common::DivCeil(program.local_memory_size, 4U)};
    const Id type{TypeArray(U32[1], Const(num_elements))};
    const Id pointer{TypePointer(spv::StorageClass::Private, type)};
    local_memory = AddGlobalVariable(pointer, spv::StorageClass::Private);
    if (profile.supported_spirv >= 0x00010400) {
        interfaces.push_back(local_memory);
    }
}

void EmitContext::DefineSharedMemory(const IR::Program& program) {
    if (program.shared_memory_size == 0) {
        return;
    }
    const auto make{[&](Id element_type, u32 element_size) {
        const u32 num_elements{Common::DivCeil(program.shared_memory_size, element_size)};
        const Id array_type{TypeArray(element_type, Const(num_elements))};
        Decorate(array_type, spv::Decoration::ArrayStride, element_size);

        const Id struct_type{TypeStruct(array_type)};
        MemberDecorate(struct_type, 0U, spv::Decoration::Offset, 0U);
        Decorate(struct_type, spv::Decoration::Block);

        const Id pointer{TypePointer(spv::StorageClass::Workgroup, struct_type)};
        const Id element_pointer{TypePointer(spv::StorageClass::Workgroup, element_type)};
        const Id variable{AddGlobalVariable(pointer, spv::StorageClass::Workgroup)};
        Decorate(variable, spv::Decoration::Aliased);
        interfaces.push_back(variable);

        return std::make_tuple(variable, element_pointer, pointer);
    }};
    if (profile.support_explicit_workgroup_layout) {
        AddExtension("SPV_KHR_workgroup_memory_explicit_layout");
        AddCapability(spv::Capability::WorkgroupMemoryExplicitLayoutKHR);
        if (program.info.uses_int8) {
            AddCapability(spv::Capability::WorkgroupMemoryExplicitLayout8BitAccessKHR);
            std::tie(shared_memory_u8, shared_u8, std::ignore) = make(U8, 1);
        }
        if (program.info.uses_int16) {
            AddCapability(spv::Capability::WorkgroupMemoryExplicitLayout16BitAccessKHR);
            std::tie(shared_memory_u16, shared_u16, std::ignore) = make(U16, 2);
        }
        if (program.info.uses_int64) {
            std::tie(shared_memory_u64, shared_u64, std::ignore) = make(U64, 8);
        }
        std::tie(shared_memory_u32, shared_u32, shared_memory_u32_type) = make(U32[1], 4);
        std::tie(shared_memory_u32x2, shared_u32x2, std::ignore) = make(U32[2], 8);
        std::tie(shared_memory_u32x4, shared_u32x4, std::ignore) = make(U32[4], 16);
        return;
    }
    const u32 num_elements{Common::DivCeil(program.shared_memory_size, 4U)};
    const Id type{TypeArray(U32[1], Const(num_elements))};
    shared_memory_u32_type = TypePointer(spv::StorageClass::Workgroup, type);

    shared_u32 = TypePointer(spv::StorageClass::Workgroup, U32[1]);
    shared_memory_u32 = AddGlobalVariable(shared_memory_u32_type, spv::StorageClass::Workgroup);
    interfaces.push_back(shared_memory_u32);

    const Id func_type{TypeFunction(void_id, U32[1], U32[1])};
    const auto make_function{[&](u32 mask, u32 size) {
        const Id loop_header{OpLabel()};
        const Id continue_block{OpLabel()};
        const Id merge_block{OpLabel()};

        const Id func{OpFunction(void_id, spv::FunctionControlMask::MaskNone, func_type)};
        const Id offset{OpFunctionParameter(U32[1])};
        const Id insert_value{OpFunctionParameter(U32[1])};
        AddLabel();
        OpBranch(loop_header);

        AddLabel(loop_header);
        const Id word_offset{OpShiftRightArithmetic(U32[1], offset, Const(2U))};
        const Id shift_offset{OpShiftLeftLogical(U32[1], offset, Const(3U))};
        const Id bit_offset{OpBitwiseAnd(U32[1], shift_offset, Const(mask))};
        const Id count{Const(size)};
        OpLoopMerge(merge_block, continue_block, spv::LoopControlMask::MaskNone);
        OpBranch(continue_block);

        AddLabel(continue_block);
        const Id word_pointer{OpAccessChain(shared_u32, shared_memory_u32, word_offset)};
        const Id old_value{OpLoad(U32[1], word_pointer)};
        const Id new_value{OpBitFieldInsert(U32[1], old_value, insert_value, bit_offset, count)};
        const Id atomic_res{OpAtomicCompareExchange(U32[1], word_pointer, Const(1U), u32_zero_value,
                                                    u32_zero_value, new_value, old_value)};
        const Id success{OpIEqual(U1, atomic_res, old_value)};
        OpBranchConditional(success, merge_block, loop_header);

        AddLabel(merge_block);
        OpReturn();
        OpFunctionEnd();
        return func;
    }};
    if (program.info.uses_int8) {
        shared_store_u8_func = make_function(24, 8);
    }
    if (program.info.uses_int16) {
        shared_store_u16_func = make_function(16, 16);
    }
}

void EmitContext::DefineSharedMemoryFunctions(const IR::Program& program) {
    if (program.info.uses_shared_increment) {
        increment_cas_shared = CasLoop(*this, Operation::Increment, shared_memory_u32_type,
                                       shared_u32, U32[1], U32[1], spv::Scope::Workgroup);
    }
    if (program.info.uses_shared_decrement) {
        decrement_cas_shared = CasLoop(*this, Operation::Decrement, shared_memory_u32_type,
                                       shared_u32, U32[1], U32[1], spv::Scope::Workgroup);
    }
}

void EmitContext::DefineAttributeMemAccess(const Info& info) {
    const auto make_load{[&] {
        const bool is_array{stage == Stage::Geometry};
        const Id end_block{OpLabel()};
        const Id default_label{OpLabel()};

        const Id func_type_load{is_array ? TypeFunction(F32[1], U32[1], U32[1])
                                         : TypeFunction(F32[1], U32[1])};
        const Id func{OpFunction(F32[1], spv::FunctionControlMask::MaskNone, func_type_load)};
        const Id offset{OpFunctionParameter(U32[1])};
        const Id vertex{is_array ? OpFunctionParameter(U32[1]) : Id{}};

        AddLabel();
        const Id base_index{OpShiftRightArithmetic(U32[1], offset, Const(2U))};
        const Id masked_index{OpBitwiseAnd(U32[1], base_index, Const(3U))};
        const Id compare_index{OpShiftRightArithmetic(U32[1], base_index, Const(2U))};
        std::vector<Sirit::Literal> literals;
        std::vector<Id> labels;
        if (info.loads.AnyComponent(IR::Attribute::PositionX)) {
            literals.push_back(static_cast<u32>(IR::Attribute::PositionX) >> 2);
            labels.push_back(OpLabel());
        }
        const u32 base_attribute_value = static_cast<u32>(IR::Attribute::Generic0X) >> 2;
        for (u32 index = 0; index < static_cast<u32>(IR::NUM_GENERICS); ++index) {
            if (!info.loads.Generic(index)) {
                continue;
            }
            literals.push_back(base_attribute_value + index);
            labels.push_back(OpLabel());
        }
        OpSelectionMerge(end_block, spv::SelectionControlMask::MaskNone);
        OpSwitch(compare_index, default_label, literals, labels);
        AddLabel(default_label);
        OpReturnValue(Const(0.0f));
        size_t label_index{0};
        if (info.loads.AnyComponent(IR::Attribute::PositionX)) {
            AddLabel(labels[label_index]);
            const Id pointer{is_array
                                 ? OpAccessChain(input_f32, input_position, vertex, masked_index)
                                 : OpAccessChain(input_f32, input_position, masked_index)};
            const Id result{OpLoad(F32[1], pointer)};
            OpReturnValue(result);
            ++label_index;
        }
        for (size_t index = 0; index < IR::NUM_GENERICS; ++index) {
            if (!info.loads.Generic(index)) {
                continue;
            }
            AddLabel(labels[label_index]);
            const auto type{AttrTypes(*this, static_cast<u32>(index))};
            if (!type) {
                OpReturnValue(Const(0.0f));
                ++label_index;
                continue;
            }
            const Id generic_id{input_generics.at(index)};
            const Id pointer{is_array
                                 ? OpAccessChain(type->pointer, generic_id, vertex, masked_index)
                                 : OpAccessChain(type->pointer, generic_id, masked_index)};
            const Id value{OpLoad(type->id, pointer)};
            const Id result{type->needs_cast ? OpBitcast(F32[1], value) : value};
            OpReturnValue(result);
            ++label_index;
        }
        AddLabel(end_block);
        OpUnreachable();
        OpFunctionEnd();
        return func;
    }};
    const auto make_store{[&] {
        const Id end_block{OpLabel()};
        const Id default_label{OpLabel()};

        const Id func_type_store{TypeFunction(void_id, U32[1], F32[1])};
        const Id func{OpFunction(void_id, spv::FunctionControlMask::MaskNone, func_type_store)};
        const Id offset{OpFunctionParameter(U32[1])};
        const Id store_value{OpFunctionParameter(F32[1])};
        AddLabel();
        const Id base_index{OpShiftRightArithmetic(U32[1], offset, Const(2U))};
        const Id masked_index{OpBitwiseAnd(U32[1], base_index, Const(3U))};
        const Id compare_index{OpShiftRightArithmetic(U32[1], base_index, Const(2U))};
        std::vector<Sirit::Literal> literals;
        std::vector<Id> labels;
        if (info.stores.AnyComponent(IR::Attribute::PositionX)) {
            literals.push_back(static_cast<u32>(IR::Attribute::PositionX) >> 2);
            labels.push_back(OpLabel());
        }
        const u32 base_attribute_value = static_cast<u32>(IR::Attribute::Generic0X) >> 2;
        for (size_t index = 0; index < IR::NUM_GENERICS; ++index) {
            if (!info.stores.Generic(index)) {
                continue;
            }
            literals.push_back(base_attribute_value + static_cast<u32>(index));
            labels.push_back(OpLabel());
        }
        if (info.stores.ClipDistances()) {
            literals.push_back(static_cast<u32>(IR::Attribute::ClipDistance0) >> 2);
            labels.push_back(OpLabel());
            literals.push_back(static_cast<u32>(IR::Attribute::ClipDistance4) >> 2);
            labels.push_back(OpLabel());
        }
        OpSelectionMerge(end_block, spv::SelectionControlMask::MaskNone);
        OpSwitch(compare_index, default_label, literals, labels);
        AddLabel(default_label);
        OpReturn();
        size_t label_index{0};
        if (info.stores.AnyComponent(IR::Attribute::PositionX)) {
            AddLabel(labels[label_index]);
            const Id pointer{OpAccessChain(output_f32, output_position, masked_index)};
            OpStore(pointer, store_value);
            OpReturn();
            ++label_index;
        }
        for (size_t index = 0; index < IR::NUM_GENERICS; ++index) {
            if (!info.stores.Generic(index)) {
                continue;
            }
            if (output_generics[index][0].num_components != 4) {
                throw NotImplementedException("Physical stores and transform feedbacks");
            }
            AddLabel(labels[label_index]);
            const Id generic_id{output_generics[index][0].id};
            const Id pointer{OpAccessChain(output_f32, generic_id, masked_index)};
            OpStore(pointer, store_value);
            OpReturn();
            ++label_index;
        }
        if (info.stores.ClipDistances()) {
            AddLabel(labels[label_index]);
            const Id pointer{OpAccessChain(output_f32, clip_distances, masked_index)};
            OpStore(pointer, store_value);
            OpReturn();
            ++label_index;
            AddLabel(labels[label_index]);
            const Id fixed_index{OpIAdd(U32[1], masked_index, Const(4U))};
            const Id pointer2{OpAccessChain(output_f32, clip_distances, fixed_index)};
            OpStore(pointer2, store_value);
            OpReturn();
            ++label_index;
        }
        AddLabel(end_block);
        OpUnreachable();
        OpFunctionEnd();
        return func;
    }};
    if (info.loads_indexed_attributes) {
        indexed_load_func = make_load();
    }
    if (info.stores_indexed_attributes) {
        indexed_store_func = make_store();
    }
}

void EmitContext::DefineGlobalMemoryFunctions(const Info& info) {
    if (!info.uses_global_memory || !profile.support_int64) {
        return;
    }
    using DefPtr = Id StorageDefinitions::*;
    const Id zero{u32_zero_value};
    const auto define_body{[&](DefPtr ssbo_member, Id addr, Id element_pointer, u32 shift,
                               auto&& callback) {
        AddLabel();
        const size_t num_buffers{info.storage_buffers_descriptors.size()};
        for (size_t index = 0; index < num_buffers; ++index) {
            if (!info.nvn_buffer_used[index]) {
                continue;
            }
            const auto& ssbo{info.storage_buffers_descriptors[index]};
            const Id ssbo_addr_cbuf_offset{Const(ssbo.cbuf_offset / 8)};
            const Id ssbo_size_cbuf_offset{Const(ssbo.cbuf_offset / 4 + 2)};
            const Id ssbo_addr_pointer{OpAccessChain(
                uniform_types.U32x2, cbufs[ssbo.cbuf_index].U32x2, zero, ssbo_addr_cbuf_offset)};
            const Id ssbo_size_pointer{OpAccessChain(uniform_types.U32, cbufs[ssbo.cbuf_index].U32,
                                                     zero, ssbo_size_cbuf_offset)};

            const Id ssbo_addr{OpBitcast(U64, OpLoad(U32[2], ssbo_addr_pointer))};
            const Id ssbo_size{OpUConvert(U64, OpLoad(U32[1], ssbo_size_pointer))};
            const Id ssbo_end{OpIAdd(U64, ssbo_addr, ssbo_size)};
            const Id cond{OpLogicalAnd(U1, OpUGreaterThanEqual(U1, addr, ssbo_addr),
                                       OpULessThan(U1, addr, ssbo_end))};
            const Id then_label{OpLabel()};
            const Id else_label{OpLabel()};
            OpSelectionMerge(else_label, spv::SelectionControlMask::MaskNone);
            OpBranchConditional(cond, then_label, else_label);
            AddLabel(then_label);
            const Id ssbo_id{ssbos[index].*ssbo_member};
            const Id ssbo_offset{OpUConvert(U32[1], OpISub(U64, addr, ssbo_addr))};
            const Id ssbo_index{OpShiftRightLogical(U32[1], ssbo_offset, Const(shift))};
            const Id ssbo_pointer{OpAccessChain(element_pointer, ssbo_id, zero, ssbo_index)};
            callback(ssbo_pointer);
            AddLabel(else_label);
        }
    }};
    const auto define_load{[&](DefPtr ssbo_member, Id element_pointer, Id type, u32 shift) {
        const Id function_type{TypeFunction(type, U64)};
        const Id func_id{OpFunction(type, spv::FunctionControlMask::MaskNone, function_type)};
        const Id addr{OpFunctionParameter(U64)};
        define_body(ssbo_member, addr, element_pointer, shift,
                    [&](Id ssbo_pointer) { OpReturnValue(OpLoad(type, ssbo_pointer)); });
        OpReturnValue(ConstantNull(type));
        OpFunctionEnd();
        return func_id;
    }};
    const auto define_write{[&](DefPtr ssbo_member, Id element_pointer, Id type, u32 shift) {
        const Id function_type{TypeFunction(void_id, U64, type)};
        const Id func_id{OpFunction(void_id, spv::FunctionControlMask::MaskNone, function_type)};
        const Id addr{OpFunctionParameter(U64)};
        const Id data{OpFunctionParameter(type)};
        define_body(ssbo_member, addr, element_pointer, shift, [&](Id ssbo_pointer) {
            OpStore(ssbo_pointer, data);
            OpReturn();
        });
        OpReturn();
        OpFunctionEnd();
        return func_id;
    }};
    const auto define{
        [&](DefPtr ssbo_member, const StorageTypeDefinition& type_def, Id type, size_t size) {
            const Id element_type{type_def.element};
            const u32 shift{static_cast<u32>(std::countr_zero(size))};
            const Id load_func{define_load(ssbo_member, element_type, type, shift)};
            const Id write_func{define_write(ssbo_member, element_type, type, shift)};
            return std::make_pair(load_func, write_func);
        }};
    std::tie(load_global_func_u32, write_global_func_u32) =
        define(&StorageDefinitions::U32, storage_types.U32, U32[1], sizeof(u32));
    std::tie(load_global_func_u32x2, write_global_func_u32x2) =
        define(&StorageDefinitions::U32x2, storage_types.U32x2, U32[2], sizeof(u32[2]));
    std::tie(load_global_func_u32x4, write_global_func_u32x4) =
        define(&StorageDefinitions::U32x4, storage_types.U32x4, U32[4], sizeof(u32[4]));
}

void EmitContext::DefineConstantBuffers(const Info& info, u32& binding) {
    if (info.constant_buffer_descriptors.empty()) {
        return;
    }
    if (!profile.support_descriptor_aliasing) {
        DefineConstBuffers(*this, info, &UniformDefinitions::U32x4, binding, U32[4], 'u',
                           sizeof(u32[4]));
        for (const ConstantBufferDescriptor& desc : info.constant_buffer_descriptors) {
            binding += desc.count;
        }
        return;
    }
    IR::Type types{info.used_constant_buffer_types};
    if (True(types & IR::Type::U8)) {
        if (profile.support_int8) {
            DefineConstBuffers(*this, info, &UniformDefinitions::U8, binding, U8, 'u', sizeof(u8));
            DefineConstBuffers(*this, info, &UniformDefinitions::S8, binding, S8, 's', sizeof(s8));
        } else {
            types |= IR::Type::U32;
        }
    }
    if (True(types & IR::Type::U16)) {
        if (profile.support_int16) {
            DefineConstBuffers(*this, info, &UniformDefinitions::U16, binding, U16, 'u',
                               sizeof(u16));
            DefineConstBuffers(*this, info, &UniformDefinitions::S16, binding, S16, 's',
                               sizeof(s16));
        } else {
            types |= IR::Type::U32;
        }
    }
    if (True(types & IR::Type::U32)) {
        DefineConstBuffers(*this, info, &UniformDefinitions::U32, binding, U32[1], 'u',
                           sizeof(u32));
    }
    if (True(types & IR::Type::F32)) {
        DefineConstBuffers(*this, info, &UniformDefinitions::F32, binding, F32[1], 'f',
                           sizeof(f32));
    }
    if (True(types & IR::Type::U32x2)) {
        DefineConstBuffers(*this, info, &UniformDefinitions::U32x2, binding, U32[2], 'u',
                           sizeof(u32[2]));
    }
    binding += static_cast<u32>(info.constant_buffer_descriptors.size());
}

void EmitContext::DefineStorageBuffers(const Info& info, u32& binding) {
    if (info.storage_buffers_descriptors.empty()) {
        return;
    }
    AddExtension("SPV_KHR_storage_buffer_storage_class");

    const IR::Type used_types{profile.support_descriptor_aliasing ? info.used_storage_buffer_types
                                                                  : IR::Type::U32};
    if (profile.support_int8 && True(used_types & IR::Type::U8)) {
        DefineSsbos(*this, storage_types.U8, &StorageDefinitions::U8, info, binding, U8,
                    sizeof(u8));
        DefineSsbos(*this, storage_types.S8, &StorageDefinitions::S8, info, binding, S8,
                    sizeof(u8));
    }
    if (profile.support_int16 && True(used_types & IR::Type::U16)) {
        DefineSsbos(*this, storage_types.U16, &StorageDefinitions::U16, info, binding, U16,
                    sizeof(u16));
        DefineSsbos(*this, storage_types.S16, &StorageDefinitions::S16, info, binding, S16,
                    sizeof(u16));
    }
    if (True(used_types & IR::Type::U32)) {
        DefineSsbos(*this, storage_types.U32, &StorageDefinitions::U32, info, binding, U32[1],
                    sizeof(u32));
    }
    if (True(used_types & IR::Type::F32)) {
        DefineSsbos(*this, storage_types.F32, &StorageDefinitions::F32, info, binding, F32[1],
                    sizeof(f32));
    }
    if (True(used_types & IR::Type::U64)) {
        DefineSsbos(*this, storage_types.U64, &StorageDefinitions::U64, info, binding, U64,
                    sizeof(u64));
    }
    if (True(used_types & IR::Type::U32x2)) {
        DefineSsbos(*this, storage_types.U32x2, &StorageDefinitions::U32x2, info, binding, U32[2],
                    sizeof(u32[2]));
    }
    if (True(used_types & IR::Type::U32x4)) {
        DefineSsbos(*this, storage_types.U32x4, &StorageDefinitions::U32x4, info, binding, U32[4],
                    sizeof(u32[4]));
    }
    for (const StorageBufferDescriptor& desc : info.storage_buffers_descriptors) {
        binding += desc.count;
    }
    const bool needs_function{
        info.uses_global_increment || info.uses_global_decrement || info.uses_atomic_f32_add ||
        info.uses_atomic_f16x2_add || info.uses_atomic_f16x2_min || info.uses_atomic_f16x2_max ||
        info.uses_atomic_f32x2_add || info.uses_atomic_f32x2_min || info.uses_atomic_f32x2_max};
    if (needs_function) {
        AddCapability(spv::Capability::VariablePointersStorageBuffer);
    }
    if (info.uses_global_increment) {
        increment_cas_ssbo = CasLoop(*this, Operation::Increment, storage_types.U32.array,
                                     storage_types.U32.element, U32[1], U32[1], spv::Scope::Device);
    }
    if (info.uses_global_decrement) {
        decrement_cas_ssbo = CasLoop(*this, Operation::Decrement, storage_types.U32.array,
                                     storage_types.U32.element, U32[1], U32[1], spv::Scope::Device);
    }
    if (info.uses_atomic_f32_add) {
        f32_add_cas = CasLoop(*this, Operation::FPAdd, storage_types.U32.array,
                              storage_types.U32.element, F32[1], U32[1], spv::Scope::Device);
    }
    if (info.uses_atomic_f16x2_add) {
        f16x2_add_cas = CasLoop(*this, Operation::FPAdd, storage_types.U32.array,
                                storage_types.U32.element, F16[2], F16[2], spv::Scope::Device);
    }
    if (info.uses_atomic_f16x2_min) {
        f16x2_min_cas = CasLoop(*this, Operation::FPMin, storage_types.U32.array,
                                storage_types.U32.element, F16[2], F16[2], spv::Scope::Device);
    }
    if (info.uses_atomic_f16x2_max) {
        f16x2_max_cas = CasLoop(*this, Operation::FPMax, storage_types.U32.array,
                                storage_types.U32.element, F16[2], F16[2], spv::Scope::Device);
    }
    if (info.uses_atomic_f32x2_add) {
        f32x2_add_cas = CasLoop(*this, Operation::FPAdd, storage_types.U32.array,
                                storage_types.U32.element, F32[2], F32[2], spv::Scope::Device);
    }
    if (info.uses_atomic_f32x2_min) {
        f32x2_min_cas = CasLoop(*this, Operation::FPMin, storage_types.U32.array,
                                storage_types.U32.element, F32[2], F32[2], spv::Scope::Device);
    }
    if (info.uses_atomic_f32x2_max) {
        f32x2_max_cas = CasLoop(*this, Operation::FPMax, storage_types.U32.array,
                                storage_types.U32.element, F32[2], F32[2], spv::Scope::Device);
    }
}

void EmitContext::DefineTextureBuffers(const Info& info, u32& binding) {
    if (info.texture_buffer_descriptors.empty()) {
        return;
    }
    const spv::ImageFormat format{spv::ImageFormat::Unknown};
    image_buffer_type = TypeImage(F32[1], spv::Dim::Buffer, 0U, false, false, 1, format);
    sampled_texture_buffer_type = TypeSampledImage(image_buffer_type);

    const Id type{TypePointer(spv::StorageClass::UniformConstant, sampled_texture_buffer_type)};
    texture_buffers.reserve(info.texture_buffer_descriptors.size());
    for (const TextureBufferDescriptor& desc : info.texture_buffer_descriptors) {
        if (desc.count != 1) {
            throw NotImplementedException("Array of texture buffers");
        }
        const Id id{AddGlobalVariable(type, spv::StorageClass::UniformConstant)};
        Decorate(id, spv::Decoration::Binding, binding);
        Decorate(id, spv::Decoration::DescriptorSet, 0U);
        Name(id, NameOf(stage, desc, "texbuf"));
        texture_buffers.push_back({
            .id = id,
            .count = desc.count,
        });
        if (profile.supported_spirv >= 0x00010400) {
            interfaces.push_back(id);
        }
        ++binding;
    }
}

void EmitContext::DefineImageBuffers(const Info& info, u32& binding) {
    image_buffers.reserve(info.image_buffer_descriptors.size());
    for (const ImageBufferDescriptor& desc : info.image_buffer_descriptors) {
        if (desc.count != 1) {
            throw NotImplementedException("Array of image buffers");
        }
        const spv::ImageFormat format{GetImageFormat(desc.format)};
        const Id image_type{TypeImage(U32[1], spv::Dim::Buffer, false, false, false, 2, format)};
        const Id pointer_type{TypePointer(spv::StorageClass::UniformConstant, image_type)};
        const Id id{AddGlobalVariable(pointer_type, spv::StorageClass::UniformConstant)};
        Decorate(id, spv::Decoration::Binding, binding);
        Decorate(id, spv::Decoration::DescriptorSet, 0U);
        Name(id, NameOf(stage, desc, "imgbuf"));
        image_buffers.push_back({
            .id = id,
            .image_type = image_type,
            .count = desc.count,
        });
        if (profile.supported_spirv >= 0x00010400) {
            interfaces.push_back(id);
        }
        ++binding;
    }
}

void EmitContext::DefineTextures(const Info& info, u32& binding) {
    textures.reserve(info.texture_descriptors.size());
    for (const TextureDescriptor& desc : info.texture_descriptors) {
        const Id image_type{ImageType(*this, desc)};
        const Id sampled_type{TypeSampledImage(image_type)};
        const Id pointer_type{TypePointer(spv::StorageClass::UniformConstant, sampled_type)};
        const Id desc_type{DescType(*this, sampled_type, pointer_type, desc.count)};
        const Id id{AddGlobalVariable(desc_type, spv::StorageClass::UniformConstant)};
        Decorate(id, spv::Decoration::Binding, binding);
        Decorate(id, spv::Decoration::DescriptorSet, 0U);
        Name(id, NameOf(stage, desc, "tex"));
        textures.push_back({
            .id = id,
            .sampled_type = sampled_type,
            .pointer_type = pointer_type,
            .image_type = image_type,
            .count = desc.count,
        });
        if (profile.supported_spirv >= 0x00010400) {
            interfaces.push_back(id);
        }
        ++binding;
    }
    if (info.uses_atomic_image_u32) {
        image_u32 = TypePointer(spv::StorageClass::Image, U32[1]);
    }
}

void EmitContext::DefineImages(const Info& info, u32& binding) {
    images.reserve(info.image_descriptors.size());
    for (const ImageDescriptor& desc : info.image_descriptors) {
        if (desc.count != 1) {
            throw NotImplementedException("Array of images");
        }
        const Id image_type{ImageType(*this, desc)};
        const Id pointer_type{TypePointer(spv::StorageClass::UniformConstant, image_type)};
        const Id id{AddGlobalVariable(pointer_type, spv::StorageClass::UniformConstant)};
        Decorate(id, spv::Decoration::Binding, binding);
        Decorate(id, spv::Decoration::DescriptorSet, 0U);
        Name(id, NameOf(stage, desc, "img"));
        images.push_back({
            .id = id,
            .image_type = image_type,
            .count = desc.count,
        });
        if (profile.supported_spirv >= 0x00010400) {
            interfaces.push_back(id);
        }
        ++binding;
    }
}

void EmitContext::DefineInputs(const IR::Program& program) {
    const Info& info{program.info};
    const VaryingState loads{info.loads.mask | info.passthrough.mask};

    if (info.uses_workgroup_id) {
        workgroup_id = DefineInput(*this, U32[3], false, spv::BuiltIn::WorkgroupId);
    }
    if (info.uses_local_invocation_id) {
        local_invocation_id = DefineInput(*this, U32[3], false, spv::BuiltIn::LocalInvocationId);
    }
    if (info.uses_invocation_id) {
        invocation_id = DefineInput(*this, U32[1], false, spv::BuiltIn::InvocationId);
    }
    if (info.uses_sample_id) {
        sample_id = DefineInput(*this, U32[1], false, spv::BuiltIn::SampleId);
    }
    if (info.uses_is_helper_invocation) {
        is_helper_invocation = DefineInput(*this, U1, false, spv::BuiltIn::HelperInvocation);
    }
    if (info.uses_subgroup_mask) {
        subgroup_mask_eq = DefineInput(*this, U32[4], false, spv::BuiltIn::SubgroupEqMaskKHR);
        subgroup_mask_lt = DefineInput(*this, U32[4], false, spv::BuiltIn::SubgroupLtMaskKHR);
        subgroup_mask_le = DefineInput(*this, U32[4], false, spv::BuiltIn::SubgroupLeMaskKHR);
        subgroup_mask_gt = DefineInput(*this, U32[4], false, spv::BuiltIn::SubgroupGtMaskKHR);
        subgroup_mask_ge = DefineInput(*this, U32[4], false, spv::BuiltIn::SubgroupGeMaskKHR);
    }
    if (info.uses_subgroup_invocation_id || info.uses_subgroup_shuffles ||
        (profile.warp_size_potentially_larger_than_guest &&
         (info.uses_subgroup_vote || info.uses_subgroup_mask))) {
        subgroup_local_invocation_id =
            DefineInput(*this, U32[1], false, spv::BuiltIn::SubgroupLocalInvocationId);
    }
    if (info.uses_fswzadd) {
        const Id f32_one{Const(1.0f)};
        const Id f32_minus_one{Const(-1.0f)};
        const Id f32_zero{Const(0.0f)};
        fswzadd_lut_a = ConstantComposite(F32[4], f32_minus_one, f32_one, f32_minus_one, f32_zero);
        fswzadd_lut_b =
            ConstantComposite(F32[4], f32_minus_one, f32_minus_one, f32_one, f32_minus_one);
    }
    if (loads[IR::Attribute::PrimitiveId]) {
        primitive_id = DefineInput(*this, U32[1], false, spv::BuiltIn::PrimitiveId);
    }
    if (loads.AnyComponent(IR::Attribute::PositionX)) {
        const bool is_fragment{stage != Stage::Fragment};
        const spv::BuiltIn built_in{is_fragment ? spv::BuiltIn::Position : spv::BuiltIn::FragCoord};
        input_position = DefineInput(*this, F32[4], true, built_in);
        if (profile.support_geometry_shader_passthrough) {
            if (info.passthrough.AnyComponent(IR::Attribute::PositionX)) {
                Decorate(input_position, spv::Decoration::PassthroughNV);
            }
        }
    }
    if (loads[IR::Attribute::InstanceId]) {
        if (profile.support_vertex_instance_id) {
            instance_id = DefineInput(*this, U32[1], true, spv::BuiltIn::InstanceId);
        } else {
            instance_index = DefineInput(*this, U32[1], true, spv::BuiltIn::InstanceIndex);
            base_instance = DefineInput(*this, U32[1], true, spv::BuiltIn::BaseInstance);
        }
    }
    if (loads[IR::Attribute::VertexId]) {
        if (profile.support_vertex_instance_id) {
            vertex_id = DefineInput(*this, U32[1], true, spv::BuiltIn::VertexId);
        } else {
            vertex_index = DefineInput(*this, U32[1], true, spv::BuiltIn::VertexIndex);
            base_vertex = DefineInput(*this, U32[1], true, spv::BuiltIn::BaseVertex);
        }
    }
    if (loads[IR::Attribute::FrontFace]) {
        front_face = DefineInput(*this, U1, true, spv::BuiltIn::FrontFacing);
    }
    if (loads[IR::Attribute::PointSpriteS] || loads[IR::Attribute::PointSpriteT]) {
        point_coord = DefineInput(*this, F32[2], true, spv::BuiltIn::PointCoord);
    }
    if (loads[IR::Attribute::TessellationEvaluationPointU] ||
        loads[IR::Attribute::TessellationEvaluationPointV]) {
        tess_coord = DefineInput(*this, F32[3], false, spv::BuiltIn::TessCoord);
    }
    for (size_t index = 0; index < IR::NUM_GENERICS; ++index) {
        const AttributeType input_type{runtime_info.generic_input_types[index]};
        if (!runtime_info.previous_stage_stores.Generic(index)) {
            continue;
        }
        if (!loads.Generic(index)) {
            continue;
        }
        if (input_type == AttributeType::Disabled) {
            continue;
        }
        const Id type{GetAttributeType(*this, input_type)};
        const Id id{DefineInput(*this, type, true)};
        Decorate(id, spv::Decoration::Location, static_cast<u32>(index));
        Name(id, fmt::format("in_attr{}", index));
        input_generics[index] = id;

        if (info.passthrough.Generic(index) && profile.support_geometry_shader_passthrough) {
            Decorate(id, spv::Decoration::PassthroughNV);
        }
        if (stage != Stage::Fragment) {
            continue;
        }
        switch (info.interpolation[index]) {
        case Interpolation::Smooth:
            // Default
            // Decorate(id, spv::Decoration::Smooth);
            break;
        case Interpolation::NoPerspective:
            Decorate(id, spv::Decoration::NoPerspective);
            break;
        case Interpolation::Flat:
            Decorate(id, spv::Decoration::Flat);
            break;
        }
    }
    if (stage == Stage::TessellationEval) {
        for (size_t index = 0; index < info.uses_patches.size(); ++index) {
            if (!info.uses_patches[index]) {
                continue;
            }
            const Id id{DefineInput(*this, F32[4], false)};
            Decorate(id, spv::Decoration::Patch);
            Decorate(id, spv::Decoration::Location, static_cast<u32>(index));
            patches[index] = id;
        }
    }
}

void EmitContext::DefineOutputs(const IR::Program& program) {
    const Info& info{program.info};
    const std::optional<u32> invocations{program.invocations};
    if (info.stores.AnyComponent(IR::Attribute::PositionX) || stage == Stage::VertexB) {
        output_position = DefineOutput(*this, F32[4], invocations, spv::BuiltIn::Position);
    }
    if (info.stores[IR::Attribute::PointSize] || runtime_info.fixed_state_point_size) {
        if (stage == Stage::Fragment) {
            throw NotImplementedException("Storing PointSize in fragment stage");
        }
        output_point_size = DefineOutput(*this, F32[1], invocations, spv::BuiltIn::PointSize);
    }
    if (info.stores.ClipDistances()) {
        if (stage == Stage::Fragment) {
            throw NotImplementedException("Storing ClipDistance in fragment stage");
        }
        const Id type{TypeArray(F32[1], Const(8U))};
        clip_distances = DefineOutput(*this, type, invocations, spv::BuiltIn::ClipDistance);
    }
    if (info.stores[IR::Attribute::Layer] &&
        (profile.support_viewport_index_layer_non_geometry || stage == Stage::Geometry)) {
        if (stage == Stage::Fragment) {
            throw NotImplementedException("Storing Layer in fragment stage");
        }
        layer = DefineOutput(*this, U32[1], invocations, spv::BuiltIn::Layer);
    }
    if (info.stores[IR::Attribute::ViewportIndex] &&
        (profile.support_viewport_index_layer_non_geometry || stage == Stage::Geometry)) {
        if (stage == Stage::Fragment) {
            throw NotImplementedException("Storing ViewportIndex in fragment stage");
        }
        viewport_index = DefineOutput(*this, U32[1], invocations, spv::BuiltIn::ViewportIndex);
    }
    if (info.stores[IR::Attribute::ViewportMask] && profile.support_viewport_mask) {
        viewport_mask = DefineOutput(*this, TypeArray(U32[1], Const(1u)), std::nullopt,
                                     spv::BuiltIn::ViewportMaskNV);
    }
    for (size_t index = 0; index < IR::NUM_GENERICS; ++index) {
        if (info.stores.Generic(index)) {
            DefineGenericOutput(*this, index, invocations);
        }
    }
    switch (stage) {
    case Stage::TessellationControl:
        if (info.stores_tess_level_outer) {
            const Id type{TypeArray(F32[1], Const(4U))};
            output_tess_level_outer =
                DefineOutput(*this, type, std::nullopt, spv::BuiltIn::TessLevelOuter);
            Decorate(output_tess_level_outer, spv::Decoration::Patch);
        }
        if (info.stores_tess_level_inner) {
            const Id type{TypeArray(F32[1], Const(2U))};
            output_tess_level_inner =
                DefineOutput(*this, type, std::nullopt, spv::BuiltIn::TessLevelInner);
            Decorate(output_tess_level_inner, spv::Decoration::Patch);
        }
        for (size_t index = 0; index < info.uses_patches.size(); ++index) {
            if (!info.uses_patches[index]) {
                continue;
            }
            const Id id{DefineOutput(*this, F32[4], std::nullopt)};
            Decorate(id, spv::Decoration::Patch);
            Decorate(id, spv::Decoration::Location, static_cast<u32>(index));
            patches[index] = id;
        }
        break;
    case Stage::Fragment:
        for (u32 index = 0; index < 8; ++index) {
            if (!info.stores_frag_color[index] && !profile.need_declared_frag_colors) {
                continue;
            }
            frag_color[index] = DefineOutput(*this, F32[4], std::nullopt);
            Decorate(frag_color[index], spv::Decoration::Location, index);
            Name(frag_color[index], fmt::format("frag_color{}", index));
        }
        if (info.stores_frag_depth) {
            frag_depth = DefineOutput(*this, F32[1], std::nullopt);
            Decorate(frag_depth, spv::Decoration::BuiltIn, spv::BuiltIn::FragDepth);
        }
        if (info.stores_sample_mask) {
            sample_mask = DefineOutput(*this, U32[1], std::nullopt);
            Decorate(sample_mask, spv::Decoration::BuiltIn, spv::BuiltIn::SampleMask);
        }
        break;
    default:
        break;
    }
}

} // namespace Shader::Backend::SPIRV
