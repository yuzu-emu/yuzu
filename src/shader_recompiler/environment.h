#pragma once

#include "common/common_types.h"

namespace Shader {

class Environment {
public:
    virtual ~Environment() = default;

    [[nodiscard]] virtual u64 ReadInstruction(u32 address) const = 0;
};

} // namespace Shader
