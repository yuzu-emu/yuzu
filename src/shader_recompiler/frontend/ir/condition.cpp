// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>

#include <fmt/format.h>

#include "shader_recompiler/frontend/ir/condition.h"

namespace Shader::IR {

std::string NameOf(Condition condition) {
    std::string ret;
    if (condition.GetFlowTest() != FlowTest::T) {
        ret = fmt::to_string(condition.GetFlowTest());
    }
    const auto [pred, negated]{condition.GetPred()};
    if (!ret.empty()) {
        ret += '&';
    }
    if (negated) {
        ret += '!';
    }
    ret += fmt::to_string(pred);
    return ret;
}

} // namespace Shader::IR
