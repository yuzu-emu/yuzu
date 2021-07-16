// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdlib>
#include <string>

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <fmt/format.h>
#include <glad/glad.h>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "core/core.h"
#include "input_common/keyboard.h"
#include "input_common/main.h"
#include "video_core/renderer_base.h"
#include "yuzu_cmd/emu_window/emu_window_sdl2_gl.h"

class SDLGLContext : public Core::Frontend::GraphicsContext {
public:
    explicit SDLGLContext(SDL_Window* window_) : window{window_} {
        context = SDL_GL_CreateContext(window);
    }

    ~SDLGLContext() {
        DoneCurrent();
        SDL_GL_DeleteContext(context);
    }

    void SwapBuffers() override {
        SDL_GL_SwapWindow(window);
    }

    void MakeCurrent() override {
        if (is_current) {
            return;
        }
        is_current = SDL_GL_MakeCurrent(window, context) == 0;
    }

    void DoneCurrent() override {
        if (!is_current) {
            return;
        }
        SDL_GL_MakeCurrent(window, nullptr);
        is_current = false;
    }

private:
    SDL_Window* window;
    SDL_GLContext context;
    bool is_current = false;
};

bool EmuWindow_SDL2_GL::SupportsRequiredGLExtensions() {
    std::vector<std::string_view> unsupported_ext;

    // Extensions required to support some texture formats.
    if (!GLAD_GL_EXT_texture_compression_s3tc) {
        unsupported_ext.push_back("EXT_texture_compression_s3tc");
    }
    if (!GLAD_GL_ARB_texture_compression_rgtc) {
        unsupported_ext.push_back("ARB_texture_compression_rgtc");
    }

    for (const auto& extension : unsupported_ext) {
        LOG_CRITICAL(Frontend, "Unsupported GL extension: {}", extension);
    }

    return unsupported_ext.empty();
}

EmuWindow_SDL2_GL::EmuWindow_SDL2_GL(InputCommon::InputSubsystem* input_subsystem, bool fullscreen)
    : EmuWindow_SDL2{input_subsystem} {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    if (Settings::values.renderer_debug) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    }
    SDL_GL_SetSwapInterval(0);

    std::string window_title = fmt::format("yuzu {} | {}-{}", Common::g_build_fullname,
                                           Common::g_scm_branch, Common::g_scm_desc);
    render_window =
        SDL_CreateWindow(window_title.c_str(),
                         SDL_WINDOWPOS_UNDEFINED, // x position
                         SDL_WINDOWPOS_UNDEFINED, // y position
                         Layout::ScreenUndocked::Width, Layout::ScreenUndocked::Height,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    if (render_window == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to create SDL2 window! {}", SDL_GetError());
        exit(1);
    }

    SetWindowIcon();

    if (fullscreen) {
        Fullscreen();
    }

    window_context = SDL_GL_CreateContext(render_window);
    core_context = CreateSharedContext();

    if (window_context == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to create SDL2 GL context: {}", SDL_GetError());
        exit(1);
    }
    if (core_context == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to create shared SDL2 GL context: {}", SDL_GetError());
        exit(1);
    }

    if (!gladLoadGLLoader(static_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
        LOG_CRITICAL(Frontend, "Failed to initialize GL functions! {}", SDL_GetError());
        exit(1);
    }

    if (!SupportsRequiredGLExtensions()) {
        LOG_CRITICAL(Frontend, "GPU does not support all required OpenGL extensions! Exiting...");
        exit(1);
    }

    OnResize();
    OnMinimalClientAreaChangeRequest(GetActiveConfig().min_client_area_size);
    SDL_PumpEvents();
    LOG_INFO(Frontend, "yuzu Version: {} | {}-{}", Common::g_build_fullname, Common::g_scm_branch,
             Common::g_scm_desc);
    Settings::LogSettings();
}

EmuWindow_SDL2_GL::~EmuWindow_SDL2_GL() {
    core_context.reset();
    SDL_GL_DeleteContext(window_context);
}

std::unique_ptr<Core::Frontend::GraphicsContext> EmuWindow_SDL2_GL::CreateSharedContext() const {
    return std::make_unique<SDLGLContext>(render_window);
}
