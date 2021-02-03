// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <boost/container/small_vector.hpp>
#include <boost/pool/pool_alloc.hpp>

#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/function.h"
#include "shader_recompiler/frontend/maxwell/control_flow.h"

namespace Shader::Maxwell {

class Program {
    friend std::string DumpProgram(const Program& program);

public:
    explicit Program(Environment& env, const Flow::CFG& cfg);

private:
    boost::pool_allocator<IR::Block, boost::default_user_allocator_new_delete,
                          boost::details::pool::null_mutex>
        block_alloc_pool;
    boost::container::small_vector<IR::Function, 1> functions;
};

[[nodiscard]] std::string DumpProgram(const Program& program);

} // namespace Shader::Maxwell
