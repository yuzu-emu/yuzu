// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <sstream>

// Ignore -Wimplicit-fallthrough due to https://github.com/libsdl-org/SDL/issues/4307
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#endif
#include <SDL.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <inih/cpp/INIReader.h>
#include "common/fs/file.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "common/param_package.h"
#include "common/settings.h"
#include "core/hle/service/acc/profile_manager.h"
#include "input_common/main.h"
#include "input_common/udp/client.h"
#include "yuzu_cmd/config.h"
#include "yuzu_cmd/default_ini.h"

namespace FS = Common::FS;

Config::Config() {
    // TODO: Don't hardcode the path; let the frontend decide where to put the config files.
    sdl2_config_loc = FS::GetYuzuPath(FS::YuzuPath::ConfigDir) / "sdl2-config.ini";
    sdl2_config = std::make_unique<INIReader>(FS::PathToUTF8String(sdl2_config_loc));

    Reload();
}

Config::~Config() = default;

bool Config::LoadINI(const std::string& default_contents, bool retry) {
    const auto config_loc_str = FS::PathToUTF8String(sdl2_config_loc);
    if (sdl2_config->ParseError() < 0) {
        if (retry) {
            LOG_WARNING(Config, "Failed to load {}. Creating file from defaults...",
                        config_loc_str);

            void(FS::CreateParentDir(sdl2_config_loc));
            void(FS::WriteStringToFile(sdl2_config_loc, FS::FileType::TextFile, default_contents));

            sdl2_config = std::make_unique<INIReader>(config_loc_str);

            return LoadINI(default_contents, false);
        }
        LOG_ERROR(Config, "Failed.");
        return false;
    }
    LOG_INFO(Config, "Successfully loaded {}", config_loc_str);
    return true;
}

static const std::array<int, Settings::NativeButton::NumButtons> default_buttons = {
    SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_T,
    SDL_SCANCODE_G, SDL_SCANCODE_F, SDL_SCANCODE_H, SDL_SCANCODE_Q, SDL_SCANCODE_W,
    SDL_SCANCODE_M, SDL_SCANCODE_N, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_B,
};

static const std::array<std::array<int, 5>, Settings::NativeAnalog::NumAnalogs> default_analogs{{
    {
        SDL_SCANCODE_UP,
        SDL_SCANCODE_DOWN,
        SDL_SCANCODE_LEFT,
        SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_D,
    },
    {
        SDL_SCANCODE_I,
        SDL_SCANCODE_K,
        SDL_SCANCODE_J,
        SDL_SCANCODE_L,
        SDL_SCANCODE_D,
    },
}};

static const std::array<int, Settings::NativeMouseButton::NumMouseButtons> default_mouse_buttons = {
    SDL_SCANCODE_LEFTBRACKET, SDL_SCANCODE_RIGHTBRACKET, SDL_SCANCODE_APOSTROPHE,
    SDL_SCANCODE_MINUS,       SDL_SCANCODE_EQUALS,
};

static const std::array<int, 0x8A> keyboard_keys = {
    0,
    0,
    0,
    0,
    SDL_SCANCODE_A,
    SDL_SCANCODE_B,
    SDL_SCANCODE_C,
    SDL_SCANCODE_D,
    SDL_SCANCODE_E,
    SDL_SCANCODE_F,
    SDL_SCANCODE_G,
    SDL_SCANCODE_H,
    SDL_SCANCODE_I,
    SDL_SCANCODE_J,
    SDL_SCANCODE_K,
    SDL_SCANCODE_L,
    SDL_SCANCODE_M,
    SDL_SCANCODE_N,
    SDL_SCANCODE_O,
    SDL_SCANCODE_P,
    SDL_SCANCODE_Q,
    SDL_SCANCODE_R,
    SDL_SCANCODE_S,
    SDL_SCANCODE_T,
    SDL_SCANCODE_U,
    SDL_SCANCODE_V,
    SDL_SCANCODE_W,
    SDL_SCANCODE_X,
    SDL_SCANCODE_Y,
    SDL_SCANCODE_Z,
    SDL_SCANCODE_1,
    SDL_SCANCODE_2,
    SDL_SCANCODE_3,
    SDL_SCANCODE_4,
    SDL_SCANCODE_5,
    SDL_SCANCODE_6,
    SDL_SCANCODE_7,
    SDL_SCANCODE_8,
    SDL_SCANCODE_9,
    SDL_SCANCODE_0,
    SDL_SCANCODE_RETURN,
    SDL_SCANCODE_ESCAPE,
    SDL_SCANCODE_BACKSPACE,
    SDL_SCANCODE_TAB,
    SDL_SCANCODE_SPACE,
    SDL_SCANCODE_MINUS,
    SDL_SCANCODE_EQUALS,
    SDL_SCANCODE_LEFTBRACKET,
    SDL_SCANCODE_RIGHTBRACKET,
    SDL_SCANCODE_BACKSLASH,
    0,
    SDL_SCANCODE_SEMICOLON,
    SDL_SCANCODE_APOSTROPHE,
    SDL_SCANCODE_GRAVE,
    SDL_SCANCODE_COMMA,
    SDL_SCANCODE_PERIOD,
    SDL_SCANCODE_SLASH,
    SDL_SCANCODE_CAPSLOCK,

    SDL_SCANCODE_F1,
    SDL_SCANCODE_F2,
    SDL_SCANCODE_F3,
    SDL_SCANCODE_F4,
    SDL_SCANCODE_F5,
    SDL_SCANCODE_F6,
    SDL_SCANCODE_F7,
    SDL_SCANCODE_F8,
    SDL_SCANCODE_F9,
    SDL_SCANCODE_F10,
    SDL_SCANCODE_F11,
    SDL_SCANCODE_F12,

    0,
    SDL_SCANCODE_SCROLLLOCK,
    SDL_SCANCODE_PAUSE,
    SDL_SCANCODE_INSERT,
    SDL_SCANCODE_HOME,
    SDL_SCANCODE_PAGEUP,
    SDL_SCANCODE_DELETE,
    SDL_SCANCODE_END,
    SDL_SCANCODE_PAGEDOWN,
    SDL_SCANCODE_RIGHT,
    SDL_SCANCODE_LEFT,
    SDL_SCANCODE_DOWN,
    SDL_SCANCODE_UP,

    SDL_SCANCODE_NUMLOCKCLEAR,
    SDL_SCANCODE_KP_DIVIDE,
    SDL_SCANCODE_KP_MULTIPLY,
    SDL_SCANCODE_KP_MINUS,
    SDL_SCANCODE_KP_PLUS,
    SDL_SCANCODE_KP_ENTER,
    SDL_SCANCODE_KP_1,
    SDL_SCANCODE_KP_2,
    SDL_SCANCODE_KP_3,
    SDL_SCANCODE_KP_4,
    SDL_SCANCODE_KP_5,
    SDL_SCANCODE_KP_6,
    SDL_SCANCODE_KP_7,
    SDL_SCANCODE_KP_8,
    SDL_SCANCODE_KP_9,
    SDL_SCANCODE_KP_0,
    SDL_SCANCODE_KP_PERIOD,

    0,
    0,
    SDL_SCANCODE_POWER,
    SDL_SCANCODE_KP_EQUALS,

    SDL_SCANCODE_F13,
    SDL_SCANCODE_F14,
    SDL_SCANCODE_F15,
    SDL_SCANCODE_F16,
    SDL_SCANCODE_F17,
    SDL_SCANCODE_F18,
    SDL_SCANCODE_F19,
    SDL_SCANCODE_F20,
    SDL_SCANCODE_F21,
    SDL_SCANCODE_F22,
    SDL_SCANCODE_F23,
    SDL_SCANCODE_F24,

    0,
    SDL_SCANCODE_HELP,
    SDL_SCANCODE_MENU,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    SDL_SCANCODE_KP_COMMA,
    SDL_SCANCODE_KP_LEFTPAREN,
    SDL_SCANCODE_KP_RIGHTPAREN,
    0,
    0,
    0,
    0,
};

static const std::array<int, 8> keyboard_mods{
    SDL_SCANCODE_LCTRL, SDL_SCANCODE_LSHIFT, SDL_SCANCODE_LALT, SDL_SCANCODE_LGUI,
    SDL_SCANCODE_RCTRL, SDL_SCANCODE_RSHIFT, SDL_SCANCODE_RALT, SDL_SCANCODE_RGUI,
};

template <>
void Config::ReadSetting(const std::string& group, Settings::BasicSetting<std::string>& setting) {
    setting = sdl2_config->Get(group, setting.GetLabel(), setting.GetDefault());
}

template <>
void Config::ReadSetting(const std::string& group, Settings::BasicSetting<bool>& setting) {
    setting = sdl2_config->GetBoolean(group, setting.GetLabel(), setting.GetDefault());
}

template <typename Type>
void Config::ReadSetting(const std::string& group, Settings::BasicSetting<Type>& setting) {
    setting = static_cast<Type>(sdl2_config->GetInteger(group, setting.GetLabel(),
                                                        static_cast<long>(setting.GetDefault())));
}

void Config::ReadValues() {
    // Controls
    for (std::size_t p = 0; p < Settings::values.players.GetValue().size(); ++p) {
        const auto group = fmt::format("ControlsP{}", p);
        for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
            std::string default_param = InputCommon::GenerateKeyboardParam(default_buttons[i]);
            Settings::values.players.GetValue()[p].buttons[i] =
                sdl2_config->Get(group, Settings::NativeButton::mapping[i], default_param);
            if (Settings::values.players.GetValue()[p].buttons[i].empty())
                Settings::values.players.GetValue()[p].buttons[i] = default_param;
        }

        for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
            std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
                default_analogs[i][0], default_analogs[i][1], default_analogs[i][2],
                default_analogs[i][3], default_analogs[i][4], 0.5f);
            Settings::values.players.GetValue()[p].analogs[i] =
                sdl2_config->Get(group, Settings::NativeAnalog::mapping[i], default_param);
            if (Settings::values.players.GetValue()[p].analogs[i].empty())
                Settings::values.players.GetValue()[p].analogs[i] = default_param;
        }
    }

    ReadSetting("ControlsGeneral", Settings::values.mouse_enabled);
    for (int i = 0; i < Settings::NativeMouseButton::NumMouseButtons; ++i) {
        std::string default_param = InputCommon::GenerateKeyboardParam(default_mouse_buttons[i]);
        Settings::values.mouse_buttons[i] = sdl2_config->Get(
            "ControlsGeneral", std::string("mouse_") + Settings::NativeMouseButton::mapping[i],
            default_param);
        if (Settings::values.mouse_buttons[i].empty())
            Settings::values.mouse_buttons[i] = default_param;
    }

    ReadSetting("ControlsGeneral", Settings::values.motion_device);

    ReadSetting("ControlsGeneral", Settings::values.keyboard_enabled);

    ReadSetting("ControlsGeneral", Settings::values.debug_pad_enabled);
    for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
        std::string default_param = InputCommon::GenerateKeyboardParam(default_buttons[i]);
        Settings::values.debug_pad_buttons[i] = sdl2_config->Get(
            "ControlsGeneral", std::string("debug_pad_") + Settings::NativeButton::mapping[i],
            default_param);
        if (Settings::values.debug_pad_buttons[i].empty())
            Settings::values.debug_pad_buttons[i] = default_param;
    }

    for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
        std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
            default_analogs[i][0], default_analogs[i][1], default_analogs[i][2],
            default_analogs[i][3], default_analogs[i][4], 0.5f);
        Settings::values.debug_pad_analogs[i] = sdl2_config->Get(
            "ControlsGeneral", std::string("debug_pad_") + Settings::NativeAnalog::mapping[i],
            default_param);
        if (Settings::values.debug_pad_analogs[i].empty())
            Settings::values.debug_pad_analogs[i] = default_param;
    }

    ReadSetting("ControlsGeneral", Settings::values.vibration_enabled);
    ReadSetting("ControlsGeneral", Settings::values.enable_accurate_vibrations);
    ReadSetting("ControlsGeneral", Settings::values.motion_enabled);
    Settings::values.touchscreen.enabled =
        sdl2_config->GetBoolean("ControlsGeneral", "touch_enabled", true);
    Settings::values.touchscreen.rotation_angle =
        sdl2_config->GetInteger("ControlsGeneral", "touch_angle", 0);
    Settings::values.touchscreen.diameter_x =
        sdl2_config->GetInteger("ControlsGeneral", "touch_diameter_x", 15);
    Settings::values.touchscreen.diameter_y =
        sdl2_config->GetInteger("ControlsGeneral", "touch_diameter_y", 15);

    int num_touch_from_button_maps =
        sdl2_config->GetInteger("ControlsGeneral", "touch_from_button_map", 0);
    if (num_touch_from_button_maps > 0) {
        for (int i = 0; i < num_touch_from_button_maps; ++i) {
            Settings::TouchFromButtonMap map;
            map.name = sdl2_config->Get("ControlsGeneral",
                                        std::string("touch_from_button_maps_") + std::to_string(i) +
                                            std::string("_name"),
                                        "default");
            const int num_touch_maps = sdl2_config->GetInteger(
                "ControlsGeneral",
                std::string("touch_from_button_maps_") + std::to_string(i) + std::string("_count"),
                0);
            map.buttons.reserve(num_touch_maps);

            for (int j = 0; j < num_touch_maps; ++j) {
                std::string touch_mapping =
                    sdl2_config->Get("ControlsGeneral",
                                     std::string("touch_from_button_maps_") + std::to_string(i) +
                                         std::string("_bind_") + std::to_string(j),
                                     "");
                map.buttons.emplace_back(std::move(touch_mapping));
            }

            Settings::values.touch_from_button_maps.emplace_back(std::move(map));
        }
    } else {
        Settings::values.touch_from_button_maps.emplace_back(
            Settings::TouchFromButtonMap{"default", {}});
        num_touch_from_button_maps = 1;
    }
    ReadSetting("ControlsGeneral", Settings::values.use_touch_from_button);
    Settings::values.touch_from_button_map_index = std::clamp(
        Settings::values.touch_from_button_map_index.GetValue(), 0, num_touch_from_button_maps - 1);

    ReadSetting("ControlsGeneral", Settings::values.udp_input_servers);

    std::transform(keyboard_keys.begin(), keyboard_keys.end(),
                   Settings::values.keyboard_keys.begin(), InputCommon::GenerateKeyboardParam);
    std::transform(keyboard_mods.begin(), keyboard_mods.end(),
                   Settings::values.keyboard_keys.begin() +
                       Settings::NativeKeyboard::LeftControlKey,
                   InputCommon::GenerateKeyboardParam);
    std::transform(keyboard_mods.begin(), keyboard_mods.end(),
                   Settings::values.keyboard_mods.begin(), InputCommon::GenerateKeyboardParam);

    // Data Storage
    ReadSetting("Data Storage", Settings::values.use_virtual_sd);
    FS::SetYuzuPath(FS::YuzuPath::NANDDir,
                    sdl2_config->Get("Data Storage", "nand_directory",
                                     FS::GetYuzuPathString(FS::YuzuPath::NANDDir)));
    FS::SetYuzuPath(FS::YuzuPath::SDMCDir,
                    sdl2_config->Get("Data Storage", "sdmc_directory",
                                     FS::GetYuzuPathString(FS::YuzuPath::SDMCDir)));
    FS::SetYuzuPath(FS::YuzuPath::LoadDir,
                    sdl2_config->Get("Data Storage", "load_directory",
                                     FS::GetYuzuPathString(FS::YuzuPath::LoadDir)));
    FS::SetYuzuPath(FS::YuzuPath::DumpDir,
                    sdl2_config->Get("Data Storage", "dump_directory",
                                     FS::GetYuzuPathString(FS::YuzuPath::DumpDir)));
    ReadSetting("Data Storage", Settings::values.gamecard_inserted);
    ReadSetting("Data Storage", Settings::values.gamecard_current_game);
    ReadSetting("Data Storage", Settings::values.gamecard_path);

    // System
    ReadSetting("System", Settings::values.use_docked_mode);

    ReadSetting("System", Settings::values.current_user);
    Settings::values.current_user = std::clamp<int>(Settings::values.current_user.GetValue(), 0,
                                                    Service::Account::MAX_USERS - 1);

    const auto rng_seed_enabled = sdl2_config->GetBoolean("System", "rng_seed_enabled", false);
    if (rng_seed_enabled) {
        Settings::values.rng_seed.SetValue(sdl2_config->GetInteger("System", "rng_seed", 0));
    } else {
        Settings::values.rng_seed.SetValue(std::nullopt);
    }

    const auto custom_rtc_enabled = sdl2_config->GetBoolean("System", "custom_rtc_enabled", false);
    if (custom_rtc_enabled) {
        Settings::values.custom_rtc =
            std::chrono::seconds(sdl2_config->GetInteger("System", "custom_rtc", 0));
    } else {
        Settings::values.custom_rtc = std::nullopt;
    }

    ReadSetting("System", Settings::values.language_index);
    ReadSetting("System", Settings::values.time_zone_index);

    // Core
    ReadSetting("Core", Settings::values.use_multi_core);

    // Renderer
    ReadSetting("Renderer", Settings::values.renderer_backend);
    ReadSetting("Renderer", Settings::values.renderer_debug);
    ReadSetting("Renderer", Settings::values.enable_nsight_aftermath);
    ReadSetting("Renderer", Settings::values.disable_shader_loop_safety_checks);
    ReadSetting("Renderer", Settings::values.vulkan_device);

    ReadSetting("Renderer", Settings::values.aspect_ratio);
    ReadSetting("Renderer", Settings::values.max_anisotropy);
    ReadSetting("Renderer", Settings::values.use_frame_limit);
    ReadSetting("Renderer", Settings::values.frame_limit);
    ReadSetting("Renderer", Settings::values.use_disk_shader_cache);
    ReadSetting("Renderer", Settings::values.gpu_accuracy);
    ReadSetting("Renderer", Settings::values.use_asynchronous_gpu_emulation);
    ReadSetting("Renderer", Settings::values.use_vsync);
    ReadSetting("Renderer", Settings::values.disable_fps_limit);
    ReadSetting("Renderer", Settings::values.shader_backend);
    ReadSetting("Renderer", Settings::values.use_asynchronous_shaders);
    ReadSetting("Renderer", Settings::values.use_nvdec_emulation);
    ReadSetting("Renderer", Settings::values.accelerate_astc);
    ReadSetting("Renderer", Settings::values.use_fast_gpu_time);

    ReadSetting("Renderer", Settings::values.bg_red);
    ReadSetting("Renderer", Settings::values.bg_green);
    ReadSetting("Renderer", Settings::values.bg_blue);

    // Audio
    ReadSetting("Audio", Settings::values.sink_id);
    ReadSetting("Audio", Settings::values.enable_audio_stretching);
    ReadSetting("Audio", Settings::values.audio_device_id);
    ReadSetting("Audio", Settings::values.volume);

    // Miscellaneous
    // log_filter has a different default here than from common
    Settings::values.log_filter =
        sdl2_config->Get("Miscellaneous", Settings::values.log_filter.GetLabel(), "*:Trace");
    ReadSetting("Miscellaneous", Settings::values.use_dev_keys);

    // Debugging
    Settings::values.record_frame_times =
        sdl2_config->GetBoolean("Debugging", "record_frame_times", false);
    ReadSetting("Debugging", Settings::values.program_args);
    ReadSetting("Debugging", Settings::values.dump_exefs);
    ReadSetting("Debugging", Settings::values.dump_nso);
    ReadSetting("Debugging", Settings::values.enable_fs_access_log);
    ReadSetting("Debugging", Settings::values.reporting_services);
    ReadSetting("Debugging", Settings::values.quest_flag);
    ReadSetting("Debugging", Settings::values.use_debug_asserts);
    ReadSetting("Debugging", Settings::values.use_auto_stub);
    ReadSetting("Debugging", Settings::values.disable_macro_jit);

    const auto title_list = sdl2_config->Get("AddOns", "title_ids", "");
    std::stringstream ss(title_list);
    std::string line;
    while (std::getline(ss, line, '|')) {
        const auto title_id = std::stoul(line, nullptr, 16);
        const auto disabled_list = sdl2_config->Get("AddOns", "disabled_" + line, "");

        std::stringstream inner_ss(disabled_list);
        std::string inner_line;
        std::vector<std::string> out;
        while (std::getline(inner_ss, inner_line, '|')) {
            out.push_back(inner_line);
        }

        Settings::values.disabled_addons.insert_or_assign(title_id, out);
    }

    // Web Service
    ReadSetting("WebService", Settings::values.enable_telemetry);
    ReadSetting("WebService", Settings::values.web_api_url);
    ReadSetting("WebService", Settings::values.yuzu_username);
    ReadSetting("WebService", Settings::values.yuzu_token);

    // Services
    ReadSetting("Services", Settings::values.bcat_backend);
    ReadSetting("Services", Settings::values.bcat_boxcat_local);
}

void Config::Reload() {
    LoadINI(DefaultINI::sdl2_config_file);
    ReadValues();
}
