// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/ir/patch.h"

namespace Shader::IR {

bool IsGeneric(Patch patch) noexcept {
    return patch >= Patch::Component0 && patch <= Patch::Component119;
}

u32 GenericPatchIndex(Patch patch) {
    if (!IsGeneric(patch)) {
        throw InvalidArgument("Patch {} is not generic", patch);
    }
    return (static_cast<u32>(patch) - static_cast<u32>(Patch::Component0)) / 4;
}

u32 GenericPatchElement(Patch patch) {
    if (!IsGeneric(patch)) {
        throw InvalidArgument("Patch {} is not generic", patch);
    }
    return (static_cast<u32>(patch) - static_cast<u32>(Patch::Component0)) % 4;
}

} // namespace Shader::IR
