// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::IR {

class Block;

struct AbstractSyntaxNode {
    enum class Type {
        Block,
        If,
        EndIf,
        Loop,
        Repeat,
        Break,
        Return,
        Unreachable,
    };
    union Data {
        Block* block;
        struct {
            U1 cond;
            Block* body;
            Block* merge;
        } if_node;
        struct {
            Block* merge;
        } end_if;
        struct {
            Block* body;
            Block* continue_block;
            Block* merge;
        } loop;
        struct {
            U1 cond;
            Block* loop_header;
            Block* merge;
        } repeat;
        struct {
            U1 cond;
            Block* merge;
            Block* skip;
        } break_node;
    };

    Data data{};
    Type type{};
};
using AbstractSyntaxList = std::vector<AbstractSyntaxNode>;

} // namespace Shader::IR
