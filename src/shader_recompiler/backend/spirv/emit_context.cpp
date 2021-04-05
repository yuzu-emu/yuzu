// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <string_view>

#include <fmt/format.h>

#include "common/common_types.h"
#include "common/div_ceil.h"
#include "shader_recompiler/backend/spirv/emit_context.h"

namespace Shader::Backend::SPIRV {
namespace {
Id ImageType(EmitContext& ctx, const TextureDescriptor& desc) {
    const spv::ImageFormat format{spv::ImageFormat::Unknown};
    const Id type{ctx.F32[1]};
    switch (desc.type) {
    case TextureType::Color1D:
        return ctx.TypeImage(type, spv::Dim::Dim1D, false, false, false, 1, format);
    case TextureType::ColorArray1D:
        return ctx.TypeImage(type, spv::Dim::Dim1D, false, true, false, 1, format);
    case TextureType::Color2D:
        return ctx.TypeImage(type, spv::Dim::Dim2D, false, false, false, 1, format);
    case TextureType::ColorArray2D:
        return ctx.TypeImage(type, spv::Dim::Dim2D, false, true, false, 1, format);
    case TextureType::Color3D:
        return ctx.TypeImage(type, spv::Dim::Dim3D, false, false, false, 1, format);
    case TextureType::ColorCube:
        return ctx.TypeImage(type, spv::Dim::Cube, false, false, false, 1, format);
    case TextureType::ColorArrayCube:
        return ctx.TypeImage(type, spv::Dim::Cube, false, true, false, 1, format);
    case TextureType::Shadow1D:
        return ctx.TypeImage(type, spv::Dim::Dim1D, true, false, false, 1, format);
    case TextureType::ShadowArray1D:
        return ctx.TypeImage(type, spv::Dim::Dim1D, true, true, false, 1, format);
    case TextureType::Shadow2D:
        return ctx.TypeImage(type, spv::Dim::Dim2D, true, false, false, 1, format);
    case TextureType::ShadowArray2D:
        return ctx.TypeImage(type, spv::Dim::Dim2D, true, true, false, 1, format);
    case TextureType::Shadow3D:
        return ctx.TypeImage(type, spv::Dim::Dim3D, true, false, false, 1, format);
    case TextureType::ShadowCube:
        return ctx.TypeImage(type, spv::Dim::Cube, true, false, false, 1, format);
    case TextureType::ShadowArrayCube:
        return ctx.TypeImage(type, spv::Dim::Cube, true, true, false, 1, format);
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

Id DefineInput(EmitContext& ctx, Id type, std::optional<spv::BuiltIn> builtin = std::nullopt) {
    return DefineVariable(ctx, type, builtin, spv::StorageClass::Input);
}

Id DefineOutput(EmitContext& ctx, Id type, std::optional<spv::BuiltIn> builtin = std::nullopt) {
    return DefineVariable(ctx, type, builtin, spv::StorageClass::Output);
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
} // Anonymous namespace

void VectorTypes::Define(Sirit::Module& sirit_ctx, Id base_type, std::string_view name) {
    defs[0] = sirit_ctx.Name(base_type, name);

    std::array<char, 6> def_name;
    for (int i = 1; i < 4; ++i) {
        const std::string_view def_name_view(
            def_name.data(),
            fmt::format_to_n(def_name.data(), def_name.size(), "{}x{}", name, i + 1).size);
        defs[i] = sirit_ctx.Name(sirit_ctx.TypeVector(base_type, i + 1), def_name_view);
    }
}

EmitContext::EmitContext(const Profile& profile_, IR::Program& program, u32& binding)
    : Sirit::Module(profile_.supported_spirv), profile{profile_}, stage{program.stage} {
    AddCapability(spv::Capability::Shader);
    DefineCommonTypes(program.info);
    DefineCommonConstants();
    DefineInterfaces(program.info);
    DefineLocalMemory(program);
    DefineSharedMemory(program);
    DefineConstantBuffers(program.info, binding);
    DefineStorageBuffers(program.info, binding);
    DefineTextures(program.info, binding);
    DefineLabels(program);
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
        return Constant(U32[1], value.U32());
    case IR::Type::U64:
        return Constant(U64, value.U64());
    case IR::Type::F32:
        return Constant(F32[1], value.F32());
    case IR::Type::F64:
        return Constant(F64[1], value.F64());
    case IR::Type::Label:
        return value.Label()->Definition<Id>();
    default:
        throw NotImplementedException("Immediate type {}", value.Type());
    }
}

void EmitContext::DefineCommonTypes(const Info& info) {
    void_id = TypeVoid();

    U1 = Name(TypeBool(), "u1");

    F32.Define(*this, TypeFloat(32), "f32");
    U32.Define(*this, TypeInt(32, false), "u32");

    private_u32 = Name(TypePointer(spv::StorageClass::Private, U32[1]), "private_u32");

    input_f32 = Name(TypePointer(spv::StorageClass::Input, F32[1]), "input_f32");
    input_u32 = Name(TypePointer(spv::StorageClass::Input, U32[1]), "input_u32");
    input_s32 = Name(TypePointer(spv::StorageClass::Input, TypeInt(32, true)), "input_s32");

    output_f32 = Name(TypePointer(spv::StorageClass::Output, F32[1]), "output_f32");

    if (info.uses_int8) {
        AddCapability(spv::Capability::Int8);
        U8 = Name(TypeInt(8, false), "u8");
        S8 = Name(TypeInt(8, true), "s8");
    }
    if (info.uses_int16) {
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
    u32_zero_value = Constant(U32[1], 0U);
    f32_zero_value = Constant(F32[1], 0.0f);
}

void EmitContext::DefineInterfaces(const Info& info) {
    DefineInputs(info);
    DefineOutputs(info);
}

void EmitContext::DefineLocalMemory(const IR::Program& program) {
    if (program.local_memory_size == 0) {
        return;
    }
    const u32 num_elements{Common::DivCeil(program.local_memory_size, 4U)};
    const Id type{TypeArray(U32[1], Constant(U32[1], num_elements))};
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
        const Id array_type{TypeArray(element_type, Constant(U32[1], num_elements))};
        Decorate(array_type, spv::Decoration::ArrayStride, element_size);

        const Id struct_type{TypeStruct(array_type)};
        MemberDecorate(struct_type, 0U, spv::Decoration::Offset, 0U);
        Decorate(struct_type, spv::Decoration::Block);

        const Id pointer{TypePointer(spv::StorageClass::Workgroup, struct_type)};
        const Id element_pointer{TypePointer(spv::StorageClass::Workgroup, element_type)};
        const Id variable{AddGlobalVariable(pointer, spv::StorageClass::Workgroup)};
        Decorate(variable, spv::Decoration::Aliased);
        interfaces.push_back(variable);

        return std::make_pair(variable, element_pointer);
    }};
    if (profile.support_explicit_workgroup_layout) {
        AddExtension("SPV_KHR_workgroup_memory_explicit_layout");
        AddCapability(spv::Capability::WorkgroupMemoryExplicitLayoutKHR);
        if (program.info.uses_int8) {
            AddCapability(spv::Capability::WorkgroupMemoryExplicitLayout8BitAccessKHR);
            std::tie(shared_memory_u8, shared_u8) = make(U8, 1);
        }
        if (program.info.uses_int16) {
            AddCapability(spv::Capability::WorkgroupMemoryExplicitLayout16BitAccessKHR);
            std::tie(shared_memory_u16, shared_u16) = make(U16, 2);
        }
        std::tie(shared_memory_u32, shared_u32) = make(U32[1], 4);
        std::tie(shared_memory_u32x2, shared_u32x2) = make(U32[2], 8);
        std::tie(shared_memory_u32x4, shared_u32x4) = make(U32[4], 16);
        return;
    }
    const u32 num_elements{Common::DivCeil(program.shared_memory_size, 4U)};
    const Id type{TypeArray(U32[1], Constant(U32[1], num_elements))};
    const Id pointer_type{TypePointer(spv::StorageClass::Workgroup, type)};
    shared_u32 = TypePointer(spv::StorageClass::Workgroup, U32[1]);
    shared_memory_u32 = AddGlobalVariable(pointer_type, spv::StorageClass::Workgroup);
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
        const Id word_offset{OpShiftRightArithmetic(U32[1], offset, Constant(U32[1], 2U))};
        const Id shift_offset{OpShiftLeftLogical(U32[1], offset, Constant(U32[1], 3U))};
        const Id bit_offset{OpBitwiseAnd(U32[1], shift_offset, Constant(U32[1], mask))};
        const Id count{Constant(U32[1], size)};
        OpLoopMerge(merge_block, continue_block, spv::LoopControlMask::MaskNone);
        OpBranch(continue_block);

        AddLabel(continue_block);
        const Id word_pointer{OpAccessChain(shared_u32, shared_memory_u32, word_offset)};
        const Id old_value{OpLoad(U32[1], word_pointer)};
        const Id new_value{OpBitFieldInsert(U32[1], old_value, insert_value, bit_offset, count)};
        const Id atomic_res{OpAtomicCompareExchange(U32[1], word_pointer, Constant(U32[1], 1U),
                                                    u32_zero_value, u32_zero_value, new_value,
                                                    old_value)};
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

void EmitContext::DefineConstantBuffers(const Info& info, u32& binding) {
    if (info.constant_buffer_descriptors.empty()) {
        return;
    }
    if (True(info.used_constant_buffer_types & IR::Type::U8)) {
        DefineConstantBuffers(info, &UniformDefinitions::U8, binding, U8, 'u', sizeof(u8));
        DefineConstantBuffers(info, &UniformDefinitions::S8, binding, S8, 's', sizeof(s8));
    }
    if (True(info.used_constant_buffer_types & IR::Type::U16)) {
        DefineConstantBuffers(info, &UniformDefinitions::U16, binding, U16, 'u', sizeof(u16));
        DefineConstantBuffers(info, &UniformDefinitions::S16, binding, S16, 's', sizeof(s16));
    }
    if (True(info.used_constant_buffer_types & IR::Type::U32)) {
        DefineConstantBuffers(info, &UniformDefinitions::U32, binding, U32[1], 'u', sizeof(u32));
    }
    if (True(info.used_constant_buffer_types & IR::Type::F32)) {
        DefineConstantBuffers(info, &UniformDefinitions::F32, binding, F32[1], 'f', sizeof(f32));
    }
    if (True(info.used_constant_buffer_types & IR::Type::U32x2)) {
        DefineConstantBuffers(info, &UniformDefinitions::U32x2, binding, U32[2], 'u', sizeof(u64));
    }
    for (const ConstantBufferDescriptor& desc : info.constant_buffer_descriptors) {
        binding += desc.count;
    }
}

void EmitContext::DefineStorageBuffers(const Info& info, u32& binding) {
    if (info.storage_buffers_descriptors.empty()) {
        return;
    }
    AddExtension("SPV_KHR_storage_buffer_storage_class");

    const Id array_type{TypeRuntimeArray(U32[1])};
    Decorate(array_type, spv::Decoration::ArrayStride, 4U);

    const Id struct_type{TypeStruct(array_type)};
    Name(struct_type, "ssbo_block");
    Decorate(struct_type, spv::Decoration::Block);
    MemberName(struct_type, 0, "data");
    MemberDecorate(struct_type, 0, spv::Decoration::Offset, 0U);

    const Id storage_type{TypePointer(spv::StorageClass::StorageBuffer, struct_type)};
    storage_u32 = TypePointer(spv::StorageClass::StorageBuffer, U32[1]);

    u32 index{};
    for (const StorageBufferDescriptor& desc : info.storage_buffers_descriptors) {
        const Id id{AddGlobalVariable(storage_type, spv::StorageClass::StorageBuffer)};
        Decorate(id, spv::Decoration::Binding, binding);
        Decorate(id, spv::Decoration::DescriptorSet, 0U);
        Name(id, fmt::format("ssbo{}", index));
        if (profile.supported_spirv >= 0x00010400) {
            interfaces.push_back(id);
        }
        std::fill_n(ssbos.data() + index, desc.count, id);
        index += desc.count;
        binding += desc.count;
    }
}

void EmitContext::DefineTextures(const Info& info, u32& binding) {
    textures.reserve(info.texture_descriptors.size());
    for (const TextureDescriptor& desc : info.texture_descriptors) {
        if (desc.count != 1) {
            throw NotImplementedException("Array of textures");
        }
        const Id image_type{ImageType(*this, desc)};
        const Id sampled_type{TypeSampledImage(image_type)};
        const Id pointer_type{TypePointer(spv::StorageClass::UniformConstant, sampled_type)};
        const Id id{AddGlobalVariable(pointer_type, spv::StorageClass::UniformConstant)};
        Decorate(id, spv::Decoration::Binding, binding);
        Decorate(id, spv::Decoration::DescriptorSet, 0U);
        Name(id, fmt::format("tex{}_{:02x}", desc.cbuf_index, desc.cbuf_offset));
        for (u32 index = 0; index < desc.count; ++index) {
            // TODO: Pass count info
            textures.push_back(TextureDefinition{
                .id{id},
                .sampled_type{sampled_type},
                .image_type{image_type},
            });
        }
        if (profile.supported_spirv >= 0x00010400) {
            interfaces.push_back(id);
        }
        binding += desc.count;
    }
}

void EmitContext::DefineLabels(IR::Program& program) {
    for (IR::Block* const block : program.blocks) {
        block->SetDefinition(OpLabel());
    }
}

void EmitContext::DefineInputs(const Info& info) {
    if (info.uses_workgroup_id) {
        workgroup_id = DefineInput(*this, U32[3], spv::BuiltIn::WorkgroupId);
    }
    if (info.uses_local_invocation_id) {
        local_invocation_id = DefineInput(*this, U32[3], spv::BuiltIn::LocalInvocationId);
    }
    if (info.uses_subgroup_mask) {
        subgroup_mask_eq = DefineInput(*this, U32[4], spv::BuiltIn::SubgroupEqMaskKHR);
        subgroup_mask_lt = DefineInput(*this, U32[4], spv::BuiltIn::SubgroupLtMaskKHR);
        subgroup_mask_le = DefineInput(*this, U32[4], spv::BuiltIn::SubgroupLeMaskKHR);
        subgroup_mask_gt = DefineInput(*this, U32[4], spv::BuiltIn::SubgroupGtMaskKHR);
        subgroup_mask_ge = DefineInput(*this, U32[4], spv::BuiltIn::SubgroupGeMaskKHR);
    }
    if (info.uses_subgroup_invocation_id ||
        (profile.warp_size_potentially_larger_than_guest &&
         (info.uses_subgroup_vote || info.uses_subgroup_mask))) {
        subgroup_local_invocation_id =
            DefineInput(*this, U32[1], spv::BuiltIn::SubgroupLocalInvocationId);
    }
    if (info.uses_fswzadd) {
        const Id f32_one{Constant(F32[1], 1.0f)};
        const Id f32_minus_one{Constant(F32[1], -1.0f)};
        const Id f32_zero{Constant(F32[1], 0.0f)};
        fswzadd_lut_a = ConstantComposite(F32[4], f32_minus_one, f32_one, f32_minus_one, f32_zero);
        fswzadd_lut_b =
            ConstantComposite(F32[4], f32_minus_one, f32_minus_one, f32_one, f32_minus_one);
    }
    if (info.loads_position) {
        const bool is_fragment{stage != Stage::Fragment};
        const spv::BuiltIn built_in{is_fragment ? spv::BuiltIn::Position : spv::BuiltIn::FragCoord};
        input_position = DefineInput(*this, F32[4], built_in);
    }
    if (info.loads_instance_id) {
        if (profile.support_vertex_instance_id) {
            instance_id = DefineInput(*this, U32[1], spv::BuiltIn::InstanceId);
        } else {
            instance_index = DefineInput(*this, U32[1], spv::BuiltIn::InstanceIndex);
            base_instance = DefineInput(*this, U32[1], spv::BuiltIn::BaseInstance);
        }
    }
    if (info.loads_vertex_id) {
        if (profile.support_vertex_instance_id) {
            vertex_id = DefineInput(*this, U32[1], spv::BuiltIn::VertexId);
        } else {
            vertex_index = DefineInput(*this, U32[1], spv::BuiltIn::VertexIndex);
            base_vertex = DefineInput(*this, U32[1], spv::BuiltIn::BaseVertex);
        }
    }
    if (info.loads_front_face) {
        front_face = DefineInput(*this, U1, spv::BuiltIn::FrontFacing);
    }
    if (info.loads_point_coord) {
        point_coord = DefineInput(*this, F32[2], spv::BuiltIn::PointCoord);
    }
    for (size_t index = 0; index < info.input_generics.size(); ++index) {
        const InputVarying generic{info.input_generics[index]};
        if (!generic.used) {
            continue;
        }
        const AttributeType input_type{profile.generic_input_types[index]};
        if (input_type == AttributeType::Disabled) {
            continue;
        }
        const Id type{GetAttributeType(*this, input_type)};
        const Id id{DefineInput(*this, type)};
        Decorate(id, spv::Decoration::Location, static_cast<u32>(index));
        Name(id, fmt::format("in_attr{}", index));
        input_generics[index] = id;

        if (stage != Stage::Fragment) {
            continue;
        }
        switch (generic.interpolation) {
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
}

void EmitContext::DefineConstantBuffers(const Info& info, Id UniformDefinitions::*member_type,
                                        u32 binding, Id type, char type_char, u32 element_size) {
    const Id array_type{TypeArray(type, Constant(U32[1], 65536U / element_size))};
    Decorate(array_type, spv::Decoration::ArrayStride, element_size);

    const Id struct_type{TypeStruct(array_type)};
    Name(struct_type, fmt::format("cbuf_block_{}{}", type_char, element_size * CHAR_BIT));
    Decorate(struct_type, spv::Decoration::Block);
    MemberName(struct_type, 0, "data");
    MemberDecorate(struct_type, 0, spv::Decoration::Offset, 0U);

    const Id struct_pointer_type{TypePointer(spv::StorageClass::Uniform, struct_type)};
    const Id uniform_type{TypePointer(spv::StorageClass::Uniform, type)};
    uniform_types.*member_type = uniform_type;

    for (const ConstantBufferDescriptor& desc : info.constant_buffer_descriptors) {
        const Id id{AddGlobalVariable(struct_pointer_type, spv::StorageClass::Uniform)};
        Decorate(id, spv::Decoration::Binding, binding);
        Decorate(id, spv::Decoration::DescriptorSet, 0U);
        Name(id, fmt::format("c{}", desc.index));
        for (size_t i = 0; i < desc.count; ++i) {
            cbufs[desc.index + i].*member_type = id;
        }
        if (profile.supported_spirv >= 0x00010400) {
            interfaces.push_back(id);
        }
        binding += desc.count;
    }
}

void EmitContext::DefineOutputs(const Info& info) {
    if (info.stores_position || stage == Stage::VertexB) {
        output_position = DefineOutput(*this, F32[4], spv::BuiltIn::Position);
    }
    if (info.stores_point_size || profile.fixed_state_point_size) {
        if (stage == Stage::Fragment) {
            throw NotImplementedException("Storing PointSize in Fragment stage");
        }
        output_point_size = DefineOutput(*this, F32[1], spv::BuiltIn::PointSize);
    }
    if (info.stores_clip_distance) {
        if (stage == Stage::Fragment) {
            throw NotImplementedException("Storing PointSize in Fragment stage");
        }
        const Id type{TypeArray(F32[1], Constant(U32[1], 8U))};
        clip_distances = DefineOutput(*this, type, spv::BuiltIn::ClipDistance);
    }
    if (info.stores_viewport_index &&
        (profile.support_viewport_index_layer_non_geometry || stage == Shader::Stage::Geometry)) {
        if (stage == Stage::Fragment) {
            throw NotImplementedException("Storing ViewportIndex in Fragment stage");
        }
        viewport_index = DefineOutput(*this, U32[1], spv::BuiltIn::ViewportIndex);
    }
    for (size_t i = 0; i < info.stores_generics.size(); ++i) {
        if (info.stores_generics[i]) {
            output_generics[i] = DefineOutput(*this, F32[4]);
            Decorate(output_generics[i], spv::Decoration::Location, static_cast<u32>(i));
            Name(output_generics[i], fmt::format("out_attr{}", i));
        }
    }
    if (stage == Stage::Fragment) {
        for (u32 index = 0; index < 8; ++index) {
            if (!info.stores_frag_color[index]) {
                continue;
            }
            frag_color[index] = DefineOutput(*this, F32[4]);
            Decorate(frag_color[index], spv::Decoration::Location, index);
            Name(frag_color[index], fmt::format("frag_color{}", index));
        }
        if (info.stores_frag_depth) {
            frag_depth = DefineOutput(*this, F32[1]);
            Decorate(frag_depth, spv::Decoration::BuiltIn, spv::BuiltIn::FragDepth);
            Name(frag_depth, "frag_depth");
        }
    }
}

} // namespace Shader::Backend::SPIRV
