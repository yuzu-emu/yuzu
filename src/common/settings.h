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
#include <utility>
#include <vector>

#include "common/common_types.h"
#include "common/settings_input.h"
#include "input_common/udp/client.h"

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
    Auto = 0,
    Accurate = 1,
    Unsafe = 2,
};

/** The BasicSetting class is a simple resource manager. It defines a label and default value
 * alongside the actual value of the setting for simpler and less-error prone use with frontend
 * configurations. Setting a default value and label is required, though subclasses may deviate from
 * this requirement.
 */
template <typename Type>
class BasicSetting {
protected:
    BasicSetting() = default;

    /**
     * Only sets the setting to the given initializer, leaving the other members to their default
     * initializers.
     *
     * @param global_val Initial value of the setting
     */
    explicit BasicSetting(const Type& global_val) : global{global_val} {}

public:
    /**
     * Sets a default value, label, and setting value.
     *
     * @param default_val Intial value of the setting, and default value of the setting
     * @param name Label for the setting
     */
    explicit BasicSetting(const Type& default_val, const std::string& name)
        : default_value{default_val}, global{default_val}, label{name} {}
    ~BasicSetting() = default;

    /**
     *  Returns a reference to the setting's value.
     *
     * @returns A reference to the setting
     */
    [[nodiscard]] const Type& GetValue() const {
        return global;
    }

    /**
     * Sets the setting to the given value.
     *
     * @param value The desired value
     */
    void SetValue(const Type& value) {
        Type temp{value};
        std::swap(global, temp);
    }

    /**
     * Returns the value that this setting was created with.
     *
     * @returns A reference to the default value
     */
    [[nodiscard]] const Type& GetDefault() const {
        return default_value;
    }

    /**
     * Returns the label this setting was created with.
     *
     * @returns A reference to the label
     */
    [[nodiscard]] const std::string& GetLabel() const {
        return label;
    }

    /**
     * Assigns a value to the setting.
     *
     * @param value The desired setting value
     *
     * @returns A reference to the setting
     */
    const Type& operator=(const Type& value) {
        Type temp{value};
        std::swap(global, temp);
        return global;
    }

    /**
     * Returns a reference to the setting.
     *
     * @returns A reference to the setting
     */
    explicit operator const Type&() const {
        return global;
    }

protected:
    const Type default_value{}; ///< The default value
    Type global{};              ///< The setting
    const std::string label{};  ///< The setting's label
};

/**
 * The Setting class is a slightly more complex version of the BasicSetting class. This adds a
 * custom setting to switch to when a guest application specifically requires it. The effect is that
 * other components of the emulator can access the setting's intended value without any need for the
 * component to ask whether the custom or global setting is needed at the moment.
 *
 * By default, the global setting is used.
 *
 * Like the BasicSetting, this requires setting a default value and label to use.
 */
template <typename Type>
class Setting final : public BasicSetting<Type> {
public:
    /**
     * Sets a default value, label, and setting value.
     *
     * @param default_val Intial value of the setting, and default value of the setting
     * @param name Label for the setting
     */
    explicit Setting(const Type& default_val, const std::string& name)
        : BasicSetting<Type>(default_val, name) {}
    ~Setting() = default;

    /**
     * Tells this setting to represent either the global or custom setting when other member
     * functions are used.
     *
     * @param to_global Whether to use the global or custom setting.
     */
    void SetGlobal(bool to_global) {
        use_global = to_global;
    }

    /**
     * Returns whether this setting is using the global setting or not.
     *
     * @returns The global state
     */
    [[nodiscard]] bool UsingGlobal() const {
        return use_global;
    }

    /**
     * Returns either the global or custom setting depending on the values of this setting's global
     * state or if the global value was specifically requested.
     *
     * @param need_global Request global value regardless of setting's state; defaults to false
     *
     * @returns The required value of the setting
     */
    [[nodiscard]] const Type& GetValue(bool need_global = false) const {
        if (use_global || need_global) {
            return this->global;
        }
        return custom;
    }

    /**
     * Sets the current setting value depending on the global state.
     *
     * @param value The new value
     */
    void SetValue(const Type& value) {
        Type temp{value};
        if (use_global) {
            std::swap(this->global, temp);
        } else {
            std::swap(custom, temp);
        }
    }

    /**
     * Assigns the current setting value depending on the global state.
     *
     * @param value The new value
     *
     * @returns A reference to the current setting value
     */
    const Type& operator=(const Type& value) {
        Type temp{value};
        if (use_global) {
            std::swap(this->global, temp);
            return this->global;
        }
        std::swap(custom, temp);
        return custom;
    }

    /**
     * Returns the current setting value depending on the global state.
     *
     * @returns A reference to the current setting value
     */
    explicit operator const Type&() const {
        if (use_global) {
            return this->global;
        }
        return custom;
    }

private:
    bool use_global{true}; ///< The setting's global state
    Type custom{};         ///< The custom value of the setting
};

/**
 * The InputSetting class allows for getting a reference to either the global or custom members.
 * This is required as we cannot easily modify the values of user-defined types within containers
 * using the SetValue() member function found in the Setting class. The primary purpose of this
 * class is to store an array of 10 PlayerInput structs for both the global and custom setting and
 * allows for easily accessing and modifying both settings.
 */
template <typename Type>
class InputSetting final {
public:
    InputSetting() = default;
    explicit InputSetting(Type val) : BasicSetting<Type>(val) {}
    ~InputSetting() = default;
    void SetGlobal(bool to_global) {
        use_global = to_global;
    }
    [[nodiscard]] bool UsingGlobal() const {
        return use_global;
    }
    [[nodiscard]] Type& GetValue(bool need_global = false) {
        if (use_global || need_global) {
            return global;
        }
        return custom;
    }

private:
    bool use_global{true}; ///< The setting's global state
    Type global{};         ///< The setting
    Type custom{};         ///< The custom setting value
};

struct TouchFromButtonMap {
    std::string name;
    std::vector<std::string> buttons;
};

struct Values {
    // Audio
    BasicSetting<std::string> audio_device_id{"auto", "output_device"};
    BasicSetting<std::string> sink_id{"auto", "output_engine"};
    BasicSetting<bool> audio_muted{false, "audio_muted"};
    Setting<bool> enable_audio_stretching{true, "enable_audio_stretching"};
    Setting<u8> volume{100, "volume"};

    // Core
    Setting<bool> use_multi_core{true, "use_multi_core"};

    // Cpu
    Setting<CPUAccuracy> cpu_accuracy{CPUAccuracy::Auto, "cpu_accuracy"};
    // TODO: remove cpu_accuracy_first_time, migration setting added 8 July 2021
    BasicSetting<bool> cpu_accuracy_first_time{true, "cpu_accuracy_first_time"};
    BasicSetting<bool> cpu_debug_mode{false, "cpu_debug_mode"};

    BasicSetting<bool> cpuopt_page_tables{true, "cpuopt_page_tables"};
    BasicSetting<bool> cpuopt_block_linking{true, "cpuopt_block_linking"};
    BasicSetting<bool> cpuopt_return_stack_buffer{true, "cpuopt_return_stack_buffer"};
    BasicSetting<bool> cpuopt_fast_dispatcher{true, "cpuopt_fast_dispatcher"};
    BasicSetting<bool> cpuopt_context_elimination{true, "cpuopt_context_elimination"};
    BasicSetting<bool> cpuopt_const_prop{true, "cpuopt_const_prop"};
    BasicSetting<bool> cpuopt_misc_ir{true, "cpuopt_misc_ir"};
    BasicSetting<bool> cpuopt_reduce_misalign_checks{true, "cpuopt_reduce_misalign_checks"};
    BasicSetting<bool> cpuopt_fastmem{true, "cpuopt_fastmem"};

    Setting<bool> cpuopt_unsafe_unfuse_fma{true, "cpuopt_unsafe_unfuse_fma"};
    Setting<bool> cpuopt_unsafe_reduce_fp_error{true, "cpuopt_unsafe_reduce_fp_error"};
    Setting<bool> cpuopt_unsafe_ignore_standard_fpcr{true, "cpuopt_unsafe_ignore_standard_fpcr"};
    Setting<bool> cpuopt_unsafe_inaccurate_nan{true, "cpuopt_unsafe_inaccurate_nan"};
    Setting<bool> cpuopt_unsafe_fastmem_check{true, "cpuopt_unsafe_fastmem_check"};

    // Renderer
    Setting<RendererBackend> renderer_backend{RendererBackend::OpenGL, "backend"};
    BasicSetting<bool> renderer_debug{false, "debug"};
    BasicSetting<bool> enable_nsight_aftermath{false, "nsight_aftermath"};
    BasicSetting<bool> disable_shader_loop_safety_checks{false,
                                                         "disable_shader_loop_safety_checks"};
    Setting<int> vulkan_device{0, "vulkan_device"};

    Setting<u16> resolution_factor{1, "resolution_factor"};
    // *nix platforms may have issues with the borderless windowed fullscreen mode.
    // Default to exclusive fullscreen on these platforms for now.
    Setting<int> fullscreen_mode{
#ifdef _WIN32
        0,
#else
        1,
#endif
        "fullscreen_mode"};
    Setting<int> aspect_ratio{0, "aspect_ratio"};
    Setting<int> max_anisotropy{0, "max_anisotropy"};
    Setting<bool> use_frame_limit{true, "use_frame_limit"};
    Setting<u16> frame_limit{100, "frame_limit"};
    Setting<bool> use_disk_shader_cache{true, "use_disk_shader_cache"};
    Setting<GPUAccuracy> gpu_accuracy{GPUAccuracy::High, "gpu_accuracy"};
    Setting<bool> use_asynchronous_gpu_emulation{true, "use_asynchronous_gpu_emulation"};
    Setting<bool> use_nvdec_emulation{true, "use_nvdec_emulation"};
    Setting<bool> accelerate_astc{true, "accelerate_astc"};
    Setting<bool> use_vsync{true, "use_vsync"};
    BasicSetting<bool> disable_fps_limit{false, "disable_fps_limit"};
    Setting<ShaderBackend> shader_backend{ShaderBackend::GLASM, "shader_backend"};
    Setting<bool> use_asynchronous_shaders{false, "use_asynchronous_shaders"};
    Setting<bool> use_fast_gpu_time{true, "use_fast_gpu_time"};
    Setting<bool> use_caches_gc{false, "use_caches_gc"};

    Setting<u8> bg_red{0, "bg_red"};
    Setting<u8> bg_green{0, "bg_green"};
    Setting<u8> bg_blue{0, "bg_blue"};

    // System
    Setting<std::optional<u32>> rng_seed{std::optional<u32>(), "rng_seed"};
    // Measured in seconds since epoch
    std::optional<std::chrono::seconds> custom_rtc;
    // Set on game boot, reset on stop. Seconds difference between current time and `custom_rtc`
    std::chrono::seconds custom_rtc_differential;

    BasicSetting<s32> current_user{0, "current_user"};
    Setting<s32> language_index{1, "language_index"};
    Setting<s32> region_index{1, "region_index"};
    Setting<s32> time_zone_index{0, "time_zone_index"};
    Setting<s32> sound_index{1, "sound_index"};

    // Controls
    InputSetting<std::array<PlayerInput, 10>> players;

    Setting<bool> use_docked_mode{true, "use_docked_mode"};

    Setting<bool> vibration_enabled{true, "vibration_enabled"};
    Setting<bool> enable_accurate_vibrations{false, "enable_accurate_vibrations"};

    Setting<bool> motion_enabled{true, "motion_enabled"};
    BasicSetting<std::string> motion_device{"engine:motion_emu,update_period:100,sensitivity:0.01",
                                            "motion_device"};
    BasicSetting<std::string> udp_input_servers{InputCommon::CemuhookUDP::DEFAULT_SRV,
                                                "udp_input_servers"};

    BasicSetting<bool> mouse_panning{false, "mouse_panning"};
    BasicSetting<u8> mouse_panning_sensitivity{1, "mouse_panning_sensitivity"};
    BasicSetting<bool> mouse_enabled{false, "mouse_enabled"};
    std::string mouse_device;
    MouseButtonsRaw mouse_buttons;

    BasicSetting<bool> emulate_analog_keyboard{false, "emulate_analog_keyboard"};
    BasicSetting<bool> keyboard_enabled{false, "keyboard_enabled"};
    KeyboardKeysRaw keyboard_keys;
    KeyboardModsRaw keyboard_mods;

    BasicSetting<bool> debug_pad_enabled{false, "debug_pad_enabled"};
    ButtonsRaw debug_pad_buttons;
    AnalogsRaw debug_pad_analogs;

    TouchscreenInput touchscreen;

    BasicSetting<bool> use_touch_from_button{false, "use_touch_from_button"};
    BasicSetting<std::string> touch_device{"min_x:100,min_y:50,max_x:1800,max_y:850",
                                           "touch_device"};
    BasicSetting<int> touch_from_button_map_index{0, "touch_from_button_map"};
    std::vector<TouchFromButtonMap> touch_from_button_maps;

    std::atomic_bool is_device_reload_pending{true};

    // Data Storage
    BasicSetting<bool> use_virtual_sd{true, "use_virtual_sd"};
    BasicSetting<bool> gamecard_inserted{false, "gamecard_inserted"};
    BasicSetting<bool> gamecard_current_game{false, "gamecard_current_game"};
    BasicSetting<std::string> gamecard_path{std::string(), "gamecard_path"};

    // Debugging
    bool record_frame_times;
    BasicSetting<bool> use_gdbstub{false, "use_gdbstub"};
    BasicSetting<u16> gdbstub_port{0, "gdbstub_port"};
    BasicSetting<std::string> program_args{std::string(), "program_args"};
    BasicSetting<bool> dump_exefs{false, "dump_exefs"};
    BasicSetting<bool> dump_nso{false, "dump_nso"};
    BasicSetting<bool> enable_fs_access_log{false, "enable_fs_access_log"};
    BasicSetting<bool> reporting_services{false, "reporting_services"};
    BasicSetting<bool> quest_flag{false, "quest_flag"};
    BasicSetting<bool> disable_macro_jit{false, "disable_macro_jit"};
    BasicSetting<bool> extended_logging{false, "extended_logging"};
    BasicSetting<bool> use_debug_asserts{false, "use_debug_asserts"};
    BasicSetting<bool> use_auto_stub{false, "use_auto_stub"};

    // Miscellaneous
    BasicSetting<std::string> log_filter{"*:Info", "log_filter"};
    BasicSetting<bool> use_dev_keys{false, "use_dev_keys"};

    // Services
    BasicSetting<std::string> bcat_backend{"none", "bcat_backend"};
    BasicSetting<bool> bcat_boxcat_local{false, "bcat_boxcat_local"};

    // WebService
    BasicSetting<bool> enable_telemetry{true, "enable_telemetry"};
    BasicSetting<std::string> web_api_url{"https://api.yuzu-emu.org", "web_api_url"};
    BasicSetting<std::string> yuzu_username{std::string(), "yuzu_username"};
    BasicSetting<std::string> yuzu_token{std::string(), "yuzu_token"};

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
