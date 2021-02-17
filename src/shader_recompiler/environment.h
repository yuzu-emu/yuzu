#pragma once

#include <array>

#include "common/common_types.h"

namespace Shader {

class Environment {
public:
    virtual ~Environment() = default;

    [[nodiscard]] virtual u64 ReadInstruction(u32 address) = 0;

    [[nodiscard]] virtual std::array<u32, 3> WorkgroupSize() = 0;
};

} // namespace Shader
