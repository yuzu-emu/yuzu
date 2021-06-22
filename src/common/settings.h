// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "common/common_types.h"
#include "common/settings_input.h"

namespace Settings {

enum class RendererBackend : u32 {
    OpenGL = 0,
    Vulkan = 1,
};

enum class ShaderBackend : u32 {
    GLSL = 0,
    GLASM = 1,
    SPIRV = 2,
};

enum class GPUAccuracy : u32 {
    Normal = 0,
    High = 1,
    Extreme = 2,
};

enum class CPUAccuracy : u32 {
    Accurate = 0,
    Unsafe = 1,
    DebugMode = 2,
};

template <typename Type>
class Setting final {
public:
    Setting() = default;
    explicit Setting(Type val) : global{val} {}
    ~Setting() = default;
    void SetGlobal(bool to_global) {
        use_global = to_global;
    }
    bool UsingGlobal() const {
        return use_global;
    }
    Type GetValue(bool need_global = false) const {
        if (use_global || need_global) {
            return global;
        }
        return local;
    }
    void SetValue(const Type& value) {
        if (use_global) {
            global = value;
        } else {
            local = value;
        }
    }

private:
    bool use_global = true;
    Type global{};
    Type local{};
};

/**
 * The InputSetting class allows for getting a reference to either the global or local members.
 * This is required as we cannot easily modify the values of user-defined types within containers
 * using the SetValue() member function found in the Setting class. The primary purpose of this
 * class is to store an array of 10 PlayerInput structs for both the global and local (per-game)
 * setting and allows for easily accessing and modifying both settings.
 */
template <typename Type>
class InputSetting final {
public:
    InputSetting() = default;
    explicit InputSetting(Type val) : global{val} {}
    ~InputSetting() = default;
    void SetGlobal(bool to_global) {
        use_global = to_global;
    }
    bool UsingGlobal() const {
        return use_global;
    }
    Type& GetValue(bool need_global = false) {
        if (use_global || need_global) {
            return global;
        }
        return local;
    }

private:
    bool use_global = true;
    Type global{};
    Type local{};
};

struct TouchFromButtonMap {
    std::string name;
    std::vector<std::string> buttons;
};

struct Values {
    // Audio
    std::string audio_device_id;
    std::string sink_id;
    bool audio_muted;
    Setting<bool> enable_audio_stretching;
    Setting<float> volume;

    // Core
    Setting<bool> use_multi_core;

    // Cpu
    Setting<CPUAccuracy> cpu_accuracy;

    bool cpuopt_page_tables;
    bool cpuopt_block_linking;
    bool cpuopt_return_stack_buffer;
    bool cpuopt_fast_dispatcher;
    bool cpuopt_context_elimination;
    bool cpuopt_const_prop;
    bool cpuopt_misc_ir;
    bool cpuopt_reduce_misalign_checks;
    bool cpuopt_fastmem;

    Setting<bool> cpuopt_unsafe_unfuse_fma;
    Setting<bool> cpuopt_unsafe_reduce_fp_error;
    Setting<bool> cpuopt_unsafe_ignore_standard_fpcr;
    Setting<bool> cpuopt_unsafe_inaccurate_nan;
    Setting<bool> cpuopt_unsafe_fastmem_check;

    // Renderer
    Setting<RendererBackend> renderer_backend;
    bool renderer_debug;
    bool enable_nsight_aftermath;
    bool disable_shader_loop_safety_checks;
    Setting<int> vulkan_device;

    Setting<u16> resolution_factor{1};
    Setting<int> fullscreen_mode;
    Setting<int> aspect_ratio;
    Setting<int> max_anisotropy;
    Setting<bool> use_frame_limit;
    Setting<u16> frame_limit;
    Setting<bool> use_disk_shader_cache;
    Setting<GPUAccuracy> gpu_accuracy;
    Setting<bool> use_asynchronous_gpu_emulation;
    Setting<bool> use_nvdec_emulation;
    Setting<bool> accelerate_astc;
    Setting<bool> use_vsync;
    Setting<bool> disable_fps_limit;
    Setting<ShaderBackend> shader_backend;
    Setting<bool> use_asynchronous_shaders;
    Setting<bool> use_fast_gpu_time;
    Setting<bool> use_caches_gc;

    Setting<float> bg_red;
    Setting<float> bg_green;
    Setting<float> bg_blue;

    // System
    Setting<std::optional<u32>> rng_seed;
    // Measured in seconds since epoch
    std::optional<std::chrono::seconds> custom_rtc;
    // Set on game boot, reset on stop. Seconds difference between current time and `custom_rtc`
    std::chrono::seconds custom_rtc_differential;

    s32 current_user;
    Setting<s32> language_index;
    Setting<s32> region_index;
    Setting<s32> time_zone_index;
    Setting<s32> sound_index;

    // Controls
    InputSetting<std::array<PlayerInput, 10>> players;

    Setting<bool> use_docked_mode;

    Setting<bool> vibration_enabled;
    Setting<bool> enable_accurate_vibrations;

    Setting<bool> motion_enabled;
    std::string motion_device;
    std::string udp_input_servers;

    bool mouse_panning;
    float mouse_panning_sensitivity;
    bool mouse_enabled;
    std::string mouse_device;
    MouseButtonsRaw mouse_buttons;

    bool emulate_analog_keyboard;
    bool keyboard_enabled;
    KeyboardKeysRaw keyboard_keys;
    KeyboardModsRaw keyboard_mods;

    bool debug_pad_enabled;
    ButtonsRaw debug_pad_buttons;
    AnalogsRaw debug_pad_analogs;

    TouchscreenInput touchscreen;

    bool use_touch_from_button;
    std::string touch_device;
    int touch_from_button_map_index;
    std::vector<TouchFromButtonMap> touch_from_button_maps;

    std::atomic_bool is_device_reload_pending{true};

    // Data Storage
    bool use_virtual_sd;
    bool gamecard_inserted;
    bool gamecard_current_game;
    std::string gamecard_path;

    // Debugging
    bool record_frame_times;
    bool use_gdbstub;
    u16 gdbstub_port;
    std::string program_args;
    bool dump_exefs;
    bool dump_nso;
    bool enable_fs_access_log;
    bool reporting_services;
    bool quest_flag;
    bool disable_macro_jit;
    bool extended_logging;
    bool use_debug_asserts;
    bool use_auto_stub;

    // Miscellaneous
    std::string log_filter;
    bool use_dev_keys;

    // Services
    std::string bcat_backend;
    bool bcat_boxcat_local;

    // WebService
    bool enable_telemetry;
    std::string web_api_url;
    std::string yuzu_username;
    std::string yuzu_token;

    // Add-Ons
    std::map<u64, std::vector<std::string>> disabled_addons;
};

extern Values values;

bool IsConfiguringGlobal();
void SetConfiguringGlobal(bool is_global);

bool IsGPULevelExtreme();
bool IsGPULevelHigh();

bool IsFastmemEnabled();

float Volume();

std::string GetTimeZoneString();

void LogSettings();

// Restore the global state of all applicable settings in the Values struct
void RestoreGlobalState(bool is_powered_on);

} // namespace Settings
