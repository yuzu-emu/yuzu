// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <span>

#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/program.h"

namespace Shader::Optimization {

void CollectShaderInfoPass(Environment& env, IR::Program& program);
void ConstantPropagationPass(IR::Program& program);
void DeadCodeEliminationPass(IR::Program& program);
void GlobalMemoryToStorageBufferPass(IR::Program& program);
void IdentityRemovalPass(IR::Program& program);
void LowerFp16ToFp32(IR::Program& program);
void LowerInt64ToInt32(IR::Program& program);
void SsaRewritePass(IR::Program& program);
void TexturePass(Environment& env, IR::Program& program);
void VerificationPass(const IR::Program& program);

// Dual Vertex
void VertexATransformPass(IR::Program& program);
void VertexBTransformPass(IR::Program& program);
void JoinTextureInfo(Info& base, Info& source);
void JoinStorageInfo(Info& base, Info& source);

} // namespace Shader::Optimization
