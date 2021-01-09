// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <string>

#include "shader_recompiler/frontend/ir/type.h"

namespace Shader::IR {

std::string NameOf(Type type) {
    static constexpr std::array names{
        "Opaque", "Label", "Reg", "Pred", "Attribute", "U1", "U8", "U16", "U32", "U64", "ZSCO",
    };
    const size_t bits{static_cast<size_t>(type)};
    if (bits == 0) {
        return "Void";
    }
    std::string result;
    for (size_t i = 0; i < names.size(); i++) {
        if ((bits & (size_t{1} << i)) != 0) {
            if (!result.empty()) {
                result += '|';
            }
            result += names[i];
        }
    }
    return result;
}

bool AreTypesCompatible(Type lhs, Type rhs) noexcept {
    return lhs == rhs || lhs == Type::Opaque || rhs == Type::Opaque;
}

} // namespace Shader::IR
