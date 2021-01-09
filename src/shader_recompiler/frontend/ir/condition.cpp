// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>

#include <fmt/format.h>

#include "shader_recompiler/frontend/ir/condition.h"

namespace Shader::IR {

std::string NameOf(Condition condition) {
    std::string ret;
    if (condition.FlowTest() != FlowTest::T) {
        ret = fmt::to_string(condition.FlowTest());
    }
    const auto [pred, negated]{condition.Pred()};
    if (pred != Pred::PT || negated) {
        if (!ret.empty()) {
            ret += '&';
        }
        if (negated) {
            ret += '!';
        }
        ret += fmt::to_string(pred);
    }
    return ret;
}

} // namespace Shader::IR
