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
    Id type;
};

struct UniformDefinitions {
    Id U8{};
    Id S8{};
    Id U16{};
    Id S16{};
    Id U32{};
    Id F32{};
    Id U64{};
};

class EmitContext final : public Sirit::Module {
public:
    explicit EmitContext(const Profile& profile, IR::Program& program);
    ~EmitContext();

    [[nodiscard]] Id Def(const IR::Value& value);

    const Profile& profile;

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

    UniformDefinitions uniform_types;

    Id storage_u32{};

    std::array<UniformDefinitions, Info::MAX_CBUFS> cbufs{};
    std::array<Id, Info::MAX_SSBOS> ssbos{};
    std::vector<TextureDefinition> textures;

    Id workgroup_id{};
    Id local_invocation_id{};

private:
    void DefineCommonTypes(const Info& info);
    void DefineCommonConstants();
    void DefineSpecialVariables(const Info& info);
    void DefineConstantBuffers(const Info& info, u32& binding);
    void DefineConstantBuffers(const Info& info, Id UniformDefinitions::*member_type, u32 binding,
                               Id type, char type_char, u32 element_size);
    void DefineStorageBuffers(const Info& info, u32& binding);
    void DefineTextures(const Info& info, u32& binding);
    void DefineLabels(IR::Program& program);
};

} // namespace Shader::Backend::SPIRV
