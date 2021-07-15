// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/frontend/emu_window.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/maxwell/control_flow.h"

namespace OpenGL::ShaderContext {
struct ShaderPools {
    void ReleaseContents() {
        flow_block.ReleaseContents();
        block.ReleaseContents();
        inst.ReleaseContents();
    }

    Shader::ObjectPool<Shader::IR::Inst> inst;
    Shader::ObjectPool<Shader::IR::Block> block;
    Shader::ObjectPool<Shader::Maxwell::Flow::Block> flow_block;
};

struct Context {
    explicit Context(Core::Frontend::EmuWindow& emu_window)
        : gl_context{emu_window.CreateSharedContext()}, scoped{*gl_context} {}

    std::unique_ptr<Core::Frontend::GraphicsContext> gl_context;
    Core::Frontend::GraphicsContext::Scoped scoped;
    ShaderPools pools;
};

} // namespace OpenGL::ShaderContext
