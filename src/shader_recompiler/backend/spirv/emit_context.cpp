// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <string_view>

#include <fmt/format.h>

#include "common/common_types.h"
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
        return ctx.TypeImage(type, spv::Dim::Cube, false, true, false, 1, format);
    }
    throw InvalidArgument("Invalid texture type {}", desc.type);
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

EmitContext::EmitContext(const Profile& profile_, IR::Program& program)
    : Sirit::Module(0x00010000), profile{profile_} {
    AddCapability(spv::Capability::Shader);
    DefineCommonTypes(program.info);
    DefineCommonConstants();
    DefineSpecialVariables(program.info);

    u32 binding{};
    DefineConstantBuffers(program.info, binding);
    DefineStorageBuffers(program.info, binding);
    DefineTextures(program.info, binding);

    DefineLabels(program);
}

EmitContext::~EmitContext() = default;

Id EmitContext::Def(const IR::Value& value) {
    if (!value.IsImmediate()) {
        return value.Inst()->Definition<Id>();
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
    case IR::Type::F32:
        return Constant(F32[1], value.F32());
    case IR::Type::F64:
        return Constant(F64[1], value.F64());
    default:
        throw NotImplementedException("Immediate type {}", value.Type());
    }
}

void EmitContext::DefineCommonTypes(const Info& info) {
    void_id = TypeVoid();

    U1 = Name(TypeBool(), "u1");

    F32.Define(*this, TypeFloat(32), "f32");
    U32.Define(*this, TypeInt(32, false), "u32");

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
}

void EmitContext::DefineSpecialVariables(const Info& info) {
    const auto define{[this](Id type, spv::BuiltIn builtin, spv::StorageClass storage_class) {
        const Id pointer_type{TypePointer(storage_class, type)};
        const Id id{AddGlobalVariable(pointer_type, spv::StorageClass::Input)};
        Decorate(id, spv::Decoration::BuiltIn, builtin);
        return id;
    }};
    using namespace std::placeholders;
    const auto define_input{std::bind(define, _1, _2, spv::StorageClass::Input)};

    if (info.uses_workgroup_id) {
        workgroup_id = define_input(U32[3], spv::BuiltIn::WorkgroupId);
    }
    if (info.uses_local_invocation_id) {
        local_invocation_id = define_input(U32[3], spv::BuiltIn::LocalInvocationId);
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
    if (True(info.used_constant_buffer_types & IR::Type::U64)) {
        DefineConstantBuffers(info, &UniformDefinitions::U64, binding, U64, 'u', sizeof(u64));
    }
    for (const ConstantBufferDescriptor& desc : info.constant_buffer_descriptors) {
        binding += desc.count;
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
        const Id type{TypeSampledImage(ImageType(*this, desc))};
        const Id pointer_type{TypePointer(spv::StorageClass::UniformConstant, type)};
        const Id id{AddGlobalVariable(pointer_type, spv::StorageClass::UniformConstant)};
        Decorate(id, spv::Decoration::Binding, binding);
        Decorate(id, spv::Decoration::DescriptorSet, 0U);
        Name(id, fmt::format("tex{}_{:02x}", desc.cbuf_index, desc.cbuf_offset));
        for (u32 index = 0; index < desc.count; ++index) {
            // TODO: Pass count info
            textures.push_back(TextureDefinition{
                .id{id},
                .type{type},
            });
        }
        binding += desc.count;
    }
}

void EmitContext::DefineLabels(IR::Program& program) {
    for (IR::Block* const block : program.blocks) {
        block->SetDefinition(OpLabel());
    }
}

} // namespace Shader::Backend::SPIRV
