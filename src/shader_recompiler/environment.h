#pragma once

#include <array>

#include "common/common_types.h"
#include "shader_recompiler/stage.h"
#include "shader_recompiler/program_header.h"

namespace Shader {

class Environment {
public:
    virtual ~Environment() = default;

    [[nodiscard]] virtual u64 ReadInstruction(u32 address) = 0;

    [[nodiscard]] virtual u32 TextureBoundBuffer() = 0;

    [[nodiscard]] virtual std::array<u32, 3> WorkgroupSize() = 0;

    [[nodiscard]] const ProgramHeader& SPH() const noexcept {
        return sph;
    }

    [[nodiscard]] Stage ShaderStage() const noexcept {
        return stage;
    }

protected:
    ProgramHeader sph{};
    Stage stage{};
};

} // namespace Shader
