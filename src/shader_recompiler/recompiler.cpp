// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <vector>

#include "common/common_types.h"
#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/maxwell/control_flow.h"
#include "shader_recompiler/frontend/maxwell/program.h"
#include "shader_recompiler/object_pool.h"
#include "shader_recompiler/recompiler.h"

namespace Shader {

std::pair<Info, std::vector<u32>> RecompileSPIRV(const Profile& profile, Environment& env,
                                                 u32 start_address) {
    ObjectPool<Maxwell::Flow::Block> flow_block_pool;
    ObjectPool<IR::Inst> inst_pool;
    ObjectPool<IR::Block> block_pool;

    Maxwell::Flow::CFG cfg{env, flow_block_pool, start_address};
    IR::Program program{Maxwell::TranslateProgram(inst_pool, block_pool, env, cfg)};
    return {std::move(program.info), Backend::SPIRV::EmitSPIRV(profile, env, program)};
}

} // namespace Shader
