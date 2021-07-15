#pragma once

#include <array>

#include "common/common_types.h"
#include "shader_recompiler/program_header.h"
#include "shader_recompiler/stage.h"

namespace Shader {

class Environment {
public:
    virtual ~Environment() = default;

    [[nodiscard]] virtual u64 ReadInstruction(u32 address) = 0;

    [[nodiscard]] virtual u32 TextureBoundBuffer() const = 0;

    [[nodiscard]] virtual std::array<u32, 3> WorkgroupSize() const = 0;

    [[nodiscard]] const ProgramHeader& SPH() const noexcept {
        return sph;
    }

    [[nodiscard]] Stage ShaderStage() const noexcept {
        return stage;
    }

    [[nodiscard]] u32 StartAddress() const noexcept {
        return start_address;
    }

protected:
    ProgramHeader sph{};
    Stage stage{};
    u32 start_address{};
};

} // namespace Shader
