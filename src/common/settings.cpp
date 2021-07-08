// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "common/assert.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "common/settings.h"

namespace Settings {

Values values = {};
static bool configuring_global = true;

std::string GetTimeZoneString() {
    static constexpr std::array timezones{
        "auto",      "default",   "CET", "CST6CDT", "Cuba",    "EET",    "Egypt",     "Eire",
        "EST",       "EST5EDT",   "GB",  "GB-Eire", "GMT",     "GMT+0",  "GMT-0",     "GMT0",
        "Greenwich", "Hongkong",  "HST", "Iceland", "Iran",    "Israel", "Jamaica",   "Japan",
        "Kwajalein", "Libya",     "MET", "MST",     "MST7MDT", "Navajo", "NZ",        "NZ-CHAT",
        "Poland",    "Portugal",  "PRC", "PST8PDT", "ROC",     "ROK",    "Singapore", "Turkey",
        "UCT",       "Universal", "UTC", "W-SU",    "WET",     "Zulu",
    };

    const auto time_zone_index = static_cast<std::size_t>(values.time_zone_index.GetValue());
    ASSERT(time_zone_index < timezones.size());
    return timezones[time_zone_index];
}

void LogSettings() {
    const auto log_setting = [](std::string_view name, const auto& value) {
        LOG_INFO(Config, "{}: {}", name, value);
    };

    const auto log_path = [](std::string_view name, const std::filesystem::path& path) {
        LOG_INFO(Config, "{}: {}", name, Common::FS::PathToUTF8String(path));
    };

    LOG_INFO(Config, "yuzu Configuration:");
    log_setting("Controls_UseDockedMode", values.use_docked_mode.GetValue());
    log_setting("System_RngSeed", values.rng_seed.GetValue().value_or(0));
    log_setting("System_CurrentUser", values.current_user.GetValue());
    log_setting("System_LanguageIndex", values.language_index.GetValue());
    log_setting("System_RegionIndex", values.region_index.GetValue());
    log_setting("System_TimeZoneIndex", values.time_zone_index.GetValue());
    log_setting("Core_UseMultiCore", values.use_multi_core.GetValue());
    log_setting("CPU_Accuracy", values.cpu_accuracy.GetValue());
    log_setting("Renderer_UseResolutionFactor", values.resolution_factor.GetValue());
    log_setting("Renderer_UseFrameLimit", values.use_frame_limit.GetValue());
    log_setting("Renderer_FrameLimit", values.frame_limit.GetValue());
    log_setting("Renderer_UseDiskShaderCache", values.use_disk_shader_cache.GetValue());
    log_setting("Renderer_GPUAccuracyLevel", values.gpu_accuracy.GetValue());
    log_setting("Renderer_UseAsynchronousGpuEmulation",
                values.use_asynchronous_gpu_emulation.GetValue());
    log_setting("Renderer_UseNvdecEmulation", values.use_nvdec_emulation.GetValue());
    log_setting("Renderer_AccelerateASTC", values.accelerate_astc.GetValue());
    log_setting("Renderer_UseVsync", values.use_vsync.GetValue());
    log_setting("Renderer_ShaderBackend", values.shader_backend.GetValue());
    log_setting("Renderer_UseAsynchronousShaders", values.use_asynchronous_shaders.GetValue());
    log_setting("Renderer_UseGarbageCollection", values.use_caches_gc.GetValue());
    log_setting("Renderer_AnisotropicFilteringLevel", values.max_anisotropy.GetValue());
    log_setting("Audio_OutputEngine", values.sink_id.GetValue());
    log_setting("Audio_EnableAudioStretching", values.enable_audio_stretching.GetValue());
    log_setting("Audio_OutputDevice", values.audio_device_id.GetValue());
    log_setting("DataStorage_UseVirtualSd", values.use_virtual_sd.GetValue());
    log_path("DataStorage_CacheDir", Common::FS::GetYuzuPath(Common::FS::YuzuPath::CacheDir));
    log_path("DataStorage_ConfigDir", Common::FS::GetYuzuPath(Common::FS::YuzuPath::ConfigDir));
    log_path("DataStorage_LoadDir", Common::FS::GetYuzuPath(Common::FS::YuzuPath::LoadDir));
    log_path("DataStorage_NANDDir", Common::FS::GetYuzuPath(Common::FS::YuzuPath::NANDDir));
    log_path("DataStorage_SDMCDir", Common::FS::GetYuzuPath(Common::FS::YuzuPath::SDMCDir));
    log_setting("Debugging_ProgramArgs", values.program_args.GetValue());
    log_setting("Services_BCATBackend", values.bcat_backend.GetValue());
    log_setting("Services_BCATBoxcatLocal", values.bcat_boxcat_local.GetValue());
}

bool IsConfiguringGlobal() {
    return configuring_global;
}

void SetConfiguringGlobal(bool is_global) {
    configuring_global = is_global;
}

bool IsGPULevelExtreme() {
    return values.gpu_accuracy.GetValue() == GPUAccuracy::Extreme;
}

bool IsGPULevelHigh() {
    return values.gpu_accuracy.GetValue() == GPUAccuracy::Extreme ||
           values.gpu_accuracy.GetValue() == GPUAccuracy::High;
}

bool IsFastmemEnabled() {
    if (values.cpu_debug_mode) {
        return static_cast<bool>(values.cpuopt_fastmem);
    }
    return true;
}

float Volume() {
    if (values.audio_muted) {
        return 0.0f;
    }
    return values.volume.GetValue() / 100.0f;
}

void RestoreGlobalState(bool is_powered_on) {
    // If a game is running, DO NOT restore the global settings state
    if (is_powered_on) {
        return;
    }

    // Audio
    values.enable_audio_stretching.SetGlobal(true);
    values.volume.SetGlobal(true);

    // Core
    values.use_multi_core.SetGlobal(true);

    // CPU
    values.cpu_accuracy.SetGlobal(true);
    values.cpuopt_unsafe_unfuse_fma.SetGlobal(true);
    values.cpuopt_unsafe_reduce_fp_error.SetGlobal(true);
    values.cpuopt_unsafe_ignore_standard_fpcr.SetGlobal(true);
    values.cpuopt_unsafe_inaccurate_nan.SetGlobal(true);
    values.cpuopt_unsafe_fastmem_check.SetGlobal(true);

    // Renderer
    values.renderer_backend.SetGlobal(true);
    values.vulkan_device.SetGlobal(true);
    values.aspect_ratio.SetGlobal(true);
    values.max_anisotropy.SetGlobal(true);
    values.use_frame_limit.SetGlobal(true);
    values.frame_limit.SetGlobal(true);
    values.use_disk_shader_cache.SetGlobal(true);
    values.gpu_accuracy.SetGlobal(true);
    values.use_asynchronous_gpu_emulation.SetGlobal(true);
    values.use_nvdec_emulation.SetGlobal(true);
    values.accelerate_astc.SetGlobal(true);
    values.use_vsync.SetGlobal(true);
    values.shader_backend.SetGlobal(true);
    values.use_asynchronous_shaders.SetGlobal(true);
    values.use_fast_gpu_time.SetGlobal(true);
    values.use_caches_gc.SetGlobal(true);
    values.bg_red.SetGlobal(true);
    values.bg_green.SetGlobal(true);
    values.bg_blue.SetGlobal(true);

    // System
    values.language_index.SetGlobal(true);
    values.region_index.SetGlobal(true);
    values.time_zone_index.SetGlobal(true);
    values.rng_seed.SetGlobal(true);
    values.sound_index.SetGlobal(true);

    // Controls
    values.players.SetGlobal(true);
    values.use_docked_mode.SetGlobal(true);
    values.vibration_enabled.SetGlobal(true);
    values.motion_enabled.SetGlobal(true);
}

} // namespace Settings
