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

EmitContext::EmitContext(IR::Program& program) : Sirit::Module(0x00010000) {
    AddCapability(spv::Capability::Shader);
    DefineCommonTypes(program.info);
    DefineCommonConstants();
    DefineSpecialVariables(program.info);
    DefineConstantBuffers(program.info);
    DefineStorageBuffers(program.info);
    DefineLabels(program);
}

EmitContext::~EmitContext() = default;

Id EmitContext::Def(const IR::Value& value) {
    if (!value.IsImmediate()) {
        return value.Inst()->Definition<Id>();
    }
    switch (value.Type()) {
    case IR::Type::U1:
        return value.U1() ? true_value : false_value;
    case IR::Type::U32:
        return Constant(U32[1], value.U32());
    case IR::Type::F32:
        return Constant(F32[1], value.F32());
    default:
        throw NotImplementedException("Immediate type {}", value.Type());
    }
}

void EmitContext::DefineCommonTypes(const Info& info) {
    void_id = TypeVoid();

    U1 = Name(TypeBool(), "u1");

    F32.Define(*this, TypeFloat(32), "f32");
    U32.Define(*this, TypeInt(32, false), "u32");

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

void EmitContext::DefineConstantBuffers(const Info& info) {
    if (info.constant_buffer_descriptors.empty()) {
        return;
    }
    const Id array_type{TypeArray(U32[1], Constant(U32[1], 4096))};
    Decorate(array_type, spv::Decoration::ArrayStride, 16U);

    const Id struct_type{TypeStruct(array_type)};
    Name(struct_type, "cbuf_block");
    Decorate(struct_type, spv::Decoration::Block);
    MemberName(struct_type, 0, "data");
    MemberDecorate(struct_type, 0, spv::Decoration::Offset, 0U);

    const Id uniform_type{TypePointer(spv::StorageClass::Uniform, struct_type)};
    uniform_u32 = TypePointer(spv::StorageClass::Uniform, U32[1]);

    u32 binding{};
    for (const Info::ConstantBufferDescriptor& desc : info.constant_buffer_descriptors) {
        const Id id{AddGlobalVariable(uniform_type, spv::StorageClass::Uniform)};
        Decorate(id, spv::Decoration::Binding, binding);
        Decorate(id, spv::Decoration::DescriptorSet, 0U);
        Name(id, fmt::format("c{}", desc.index));
        std::fill_n(cbufs.data() + desc.index, desc.count, id);
        binding += desc.count;
    }
}

void EmitContext::DefineStorageBuffers(const Info& info) {
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

    u32 binding{};
    for (const Info::StorageBufferDescriptor& desc : info.storage_buffers_descriptors) {
        const Id id{AddGlobalVariable(storage_type, spv::StorageClass::StorageBuffer)};
        Decorate(id, spv::Decoration::Binding, binding);
        Decorate(id, spv::Decoration::DescriptorSet, 0U);
        Name(id, fmt::format("ssbo{}", binding));
        std::fill_n(ssbos.data() + binding, desc.count, id);
        binding += desc.count;
    }
}

void EmitContext::DefineLabels(IR::Program& program) {
    for (const IR::Function& function : program.functions) {
        for (IR::Block* const block : function.blocks) {
            block->SetDefinition(OpLabel());
        }
    }
}

} // namespace Shader::Backend::SPIRV
