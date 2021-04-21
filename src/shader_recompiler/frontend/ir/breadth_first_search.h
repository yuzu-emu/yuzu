// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <type_traits>
#include <queue>

#include <boost/container/small_vector.hpp>

#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::IR {

template <typename Pred>
auto BreadthFirstSearch(const Value& value, Pred&& pred)
    -> std::invoke_result_t<Pred, const Inst*> {
    if (value.IsImmediate()) {
        // Nothing to do with immediates
        return std::nullopt;
    }
    // Breadth-first search visiting the right most arguments first
    // Small vector has been determined from shaders in Super Smash Bros. Ultimate
    boost::container::small_vector<const Inst*, 2> visited;
    std::queue<const Inst*> queue;
    queue.push(value.InstRecursive());

    while (!queue.empty()) {
        // Pop one instruction from the queue
        const Inst* const inst{queue.front()};
        queue.pop();
        if (const std::optional result = pred(inst)) {
            // This is the instruction we were looking for
            return result;
        }
        // Visit the right most arguments first
        for (size_t arg = inst->NumArgs(); arg--;) {
            const Value arg_value{inst->Arg(arg)};
            if (arg_value.IsImmediate()) {
                continue;
            }
            // Queue instruction if it hasn't been visited
            const Inst* const arg_inst{arg_value.InstRecursive()};
            if (std::ranges::find(visited, arg_inst) == visited.end()) {
                visited.push_back(arg_inst);
                queue.push(arg_inst);
            }
        }
    }
    // SSA tree has been traversed and the result hasn't been found
    return std::nullopt;
}

} // namespace Shader::IR
