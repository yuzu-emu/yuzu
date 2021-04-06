// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string_view>

#include <sirit/sirit.h>

#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/shader_info.h"

namespace Shader::Backend::SPIRV {

using Sirit::Id;

class VectorTypes {
public:
    void Define(Sirit::Module& sirit_ctx, Id base_type, std::string_view name);

    [[nodiscard]] Id operator[](size_t size) const noexcept {
        return defs[size - 1];
    }

private:
    std::array<Id, 4> defs{};
};

struct TextureDefinition {
    Id id;
    Id sampled_type;
    Id image_type;
};

struct UniformDefinitions {
    Id U8{};
    Id S8{};
    Id U16{};
    Id S16{};
    Id U32{};
    Id F32{};
    Id U32x2{};
};

class EmitContext final : public Sirit::Module {
public:
    explicit EmitContext(const Profile& profile, IR::Program& program, u32& binding);
    ~EmitContext();

    [[nodiscard]] Id Def(const IR::Value& value);

    const Profile& profile;
    Stage stage{};

    Id void_id{};
    Id U1{};
    Id U8{};
    Id S8{};
    Id U16{};
    Id S16{};
    Id U64{};
    VectorTypes F32;
    VectorTypes U32;
    VectorTypes F16;
    VectorTypes F64;

    Id true_value{};
    Id false_value{};
    Id u32_zero_value{};
    Id f32_zero_value{};

    UniformDefinitions uniform_types;

    Id private_u32{};

    Id shared_u8{};
    Id shared_u16{};
    Id shared_u32{};
    Id shared_u32x2{};
    Id shared_u32x4{};

    Id input_f32{};
    Id input_u32{};
    Id input_s32{};

    Id output_f32{};

    Id storage_u32{};

    Id image_buffer_type{};
    Id sampled_texture_buffer_type{};

    std::array<UniformDefinitions, Info::MAX_CBUFS> cbufs{};
    std::array<Id, Info::MAX_SSBOS> ssbos{};
    std::vector<TextureDefinition> textures;
    std::vector<Id> texture_buffers;

    Id workgroup_id{};
    Id local_invocation_id{};
    Id subgroup_local_invocation_id{};
    Id subgroup_mask_eq{};
    Id subgroup_mask_lt{};
    Id subgroup_mask_le{};
    Id subgroup_mask_gt{};
    Id subgroup_mask_ge{};
    Id instance_id{};
    Id instance_index{};
    Id base_instance{};
    Id vertex_id{};
    Id vertex_index{};
    Id base_vertex{};
    Id front_face{};
    Id point_coord{};
    Id clip_distances{};
    Id viewport_index{};

    Id fswzadd_lut_a{};
    Id fswzadd_lut_b{};

    Id indexed_load_func{};
    Id indexed_store_func{};

    Id local_memory{};

    Id shared_memory_u8{};
    Id shared_memory_u16{};
    Id shared_memory_u32{};
    Id shared_memory_u32x2{};
    Id shared_memory_u32x4{};

    Id shared_store_u8_func{};
    Id shared_store_u16_func{};

    Id input_position{};
    std::array<Id, 32> input_generics{};

    Id output_point_size{};
    Id output_position{};
    std::array<Id, 32> output_generics{};

    std::array<Id, 8> frag_color{};
    Id frag_depth{};

    std::vector<Id> interfaces;

private:
    void DefineCommonTypes(const Info& info);
    void DefineCommonConstants();
    void DefineInterfaces(const Info& info);
    void DefineLocalMemory(const IR::Program& program);
    void DefineSharedMemory(const IR::Program& program);
    void DefineConstantBuffers(const Info& info, u32& binding);
    void DefineStorageBuffers(const Info& info, u32& binding);
    void DefineTextures(const Info& info, u32& binding);
    void DefineTextureBuffers(const Info& info, u32& binding);
    void DefineAttributeMemAccess(const Info& info);
    void DefineLabels(IR::Program& program);

    void DefineConstantBuffers(const Info& info, Id UniformDefinitions::*member_type, u32 binding,
                               Id type, char type_char, u32 element_size);

    void DefineInputs(const Info& info);
    void DefineOutputs(const Info& info);
};

} // namespace Shader::Backend::SPIRV
