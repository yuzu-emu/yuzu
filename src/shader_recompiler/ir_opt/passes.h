// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "shader_recompiler/frontend/ir/basic_block.h"

namespace Shader::Optimization {

void DeadCodeEliminationPass(IR::Block& block);
void GetSetElimination(IR::Block& block);
void IdentityRemovalPass(IR::Block& block);
void VerificationPass(const IR::Block& block);

} // namespace Shader::Optimization
