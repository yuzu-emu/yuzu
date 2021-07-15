// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/fs/file.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"

#include "common/settings.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/loader/loader.h"
#include "core/telemetry_session.h"

#ifdef ENABLE_WEB_SERVICE
#include "web_service/telemetry_json.h"
#include "web_service/verify_login.h"
#endif

namespace Core {

namespace Telemetry = Common::Telemetry;

static u64 GenerateTelemetryId() {
    u64 telemetry_id{};

    mbedtls_entropy_context entropy;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_context ctr_drbg;
    constexpr std::array<char, 18> personalization{{"yuzu Telemetry ID"}};

    mbedtls_ctr_drbg_init(&ctr_drbg);
    ASSERT(mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                 reinterpret_cast<const unsigned char*>(personalization.data()),
                                 personalization.size()) == 0);
    ASSERT(mbedtls_ctr_drbg_random(&ctr_drbg, reinterpret_cast<unsigned char*>(&telemetry_id),
                                   sizeof(u64)) == 0);

    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    return telemetry_id;
}

static const char* TranslateRenderer(Settings::RendererBackend backend) {
    switch (backend) {
    case Settings::RendererBackend::OpenGL:
        return "OpenGL";
    case Settings::RendererBackend::Vulkan:
        return "Vulkan";
    }
    return "Unknown";
}

static const char* TranslateGPUAccuracyLevel(Settings::GPUAccuracy backend) {
    switch (backend) {
    case Settings::GPUAccuracy::Normal:
        return "Normal";
    case Settings::GPUAccuracy::High:
        return "High";
    case Settings::GPUAccuracy::Extreme:
        return "Extreme";
    }
    return "Unknown";
}

u64 GetTelemetryId() {
    u64 telemetry_id{};
    const auto filename = Common::FS::GetYuzuPath(Common::FS::YuzuPath::ConfigDir) / "telemetry_id";

    bool generate_new_id = !Common::FS::Exists(filename);

    if (!generate_new_id) {
        Common::FS::IOFile file{filename, Common::FS::FileAccessMode::Read,
                                Common::FS::FileType::BinaryFile};

        if (!file.IsOpen()) {
            LOG_ERROR(Core, "failed to open telemetry_id: {}",
                      Common::FS::PathToUTF8String(filename));
            return {};
        }

        if (!file.ReadObject(telemetry_id) || telemetry_id == 0) {
            LOG_ERROR(Frontend, "telemetry_id is 0. Generating a new one.", telemetry_id);
            generate_new_id = true;
        }
    }

    if (generate_new_id) {
        Common::FS::IOFile file{filename, Common::FS::FileAccessMode::Write,
                                Common::FS::FileType::BinaryFile};

        if (!file.IsOpen()) {
            LOG_ERROR(Core, "failed to open telemetry_id: {}",
                      Common::FS::PathToUTF8String(filename));
            return {};
        }

        telemetry_id = GenerateTelemetryId();

        if (!file.WriteObject(telemetry_id)) {
            LOG_ERROR(Core, "Failed to write telemetry_id to file.");
        }
    }

    return telemetry_id;
}

u64 RegenerateTelemetryId() {
    const u64 new_telemetry_id{GenerateTelemetryId()};
    const auto filename = Common::FS::GetYuzuPath(Common::FS::YuzuPath::ConfigDir) / "telemetry_id";

    Common::FS::IOFile file{filename, Common::FS::FileAccessMode::Write,
                            Common::FS::FileType::BinaryFile};

    if (!file.IsOpen()) {
        LOG_ERROR(Core, "failed to open telemetry_id: {}", Common::FS::PathToUTF8String(filename));
        return {};
    }

    if (!file.WriteObject(new_telemetry_id)) {
        LOG_ERROR(Core, "Failed to write telemetry_id to file.");
    }

    return new_telemetry_id;
}

bool VerifyLogin(const std::string& username, const std::string& token) {
#ifdef ENABLE_WEB_SERVICE
    return WebService::VerifyLogin(Settings::values.web_api_url.GetValue(), username, token);
#else
    return false;
#endif
}

TelemetrySession::TelemetrySession() = default;

TelemetrySession::~TelemetrySession() {
    // Log one-time session end information
    const s64 shutdown_time{std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count()};
    AddField(Telemetry::FieldType::Session, "Shutdown_Time", shutdown_time);

#ifdef ENABLE_WEB_SERVICE
    auto backend = std::make_unique<WebService::TelemetryJson>(
        Settings::values.web_api_url.GetValue(), Settings::values.yuzu_username.GetValue(),
        Settings::values.yuzu_token.GetValue());
#else
    auto backend = std::make_unique<Telemetry::NullVisitor>();
#endif

    // Complete the session, submitting to the web service backend if necessary
    field_collection.Accept(*backend);
    if (Settings::values.enable_telemetry) {
        backend->Complete();
    }
}

void TelemetrySession::AddInitialInfo(Loader::AppLoader& app_loader,
                                      const Service::FileSystem::FileSystemController& fsc,
                                      const FileSys::ContentProvider& content_provider) {
    // Log one-time top-level information
    AddField(Telemetry::FieldType::None, "TelemetryId", GetTelemetryId());

    // Log one-time session start information
    const s64 init_time{std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count()};
    AddField(Telemetry::FieldType::Session, "Init_Time", init_time);

    u64 program_id{};
    const Loader::ResultStatus res{app_loader.ReadProgramId(program_id)};
    if (res == Loader::ResultStatus::Success) {
        const std::string formatted_program_id{fmt::format("{:016X}", program_id)};
        AddField(Telemetry::FieldType::Session, "ProgramId", formatted_program_id);

        std::string name;
        app_loader.ReadTitle(name);

        if (name.empty()) {
            const auto metadata = [&content_provider, &fsc, program_id] {
                const FileSys::PatchManager pm{program_id, fsc, content_provider};
                return pm.GetControlMetadata();
            }();
            if (metadata.first != nullptr) {
                name = metadata.first->GetApplicationName();
            }
        }

        if (!name.empty()) {
            AddField(Telemetry::FieldType::Session, "ProgramName", name);
        }
    }

    AddField(Telemetry::FieldType::Session, "ProgramFormat",
             static_cast<u8>(app_loader.GetFileType()));

    // Log application information
    Telemetry::AppendBuildInfo(field_collection);

    // Log user system information
    Telemetry::AppendCPUInfo(field_collection);
    Telemetry::AppendOSInfo(field_collection);

    // Log user configuration information
    constexpr auto field_type = Telemetry::FieldType::UserConfig;
    AddField(field_type, "Audio_SinkId", Settings::values.sink_id.GetValue());
    AddField(field_type, "Audio_EnableAudioStretching",
             Settings::values.enable_audio_stretching.GetValue());
    AddField(field_type, "Core_UseMultiCore", Settings::values.use_multi_core.GetValue());
    AddField(field_type, "Renderer_Backend",
             TranslateRenderer(Settings::values.renderer_backend.GetValue()));
    AddField(field_type, "Renderer_ResolutionFactor",
             Settings::values.resolution_factor.GetValue());
    AddField(field_type, "Renderer_UseFrameLimit", Settings::values.use_frame_limit.GetValue());
    AddField(field_type, "Renderer_FrameLimit", Settings::values.frame_limit.GetValue());
    AddField(field_type, "Renderer_UseDiskShaderCache",
             Settings::values.use_disk_shader_cache.GetValue());
    AddField(field_type, "Renderer_GPUAccuracyLevel",
             TranslateGPUAccuracyLevel(Settings::values.gpu_accuracy.GetValue()));
    AddField(field_type, "Renderer_UseAsynchronousGpuEmulation",
             Settings::values.use_asynchronous_gpu_emulation.GetValue());
    AddField(field_type, "Renderer_UseNvdecEmulation",
             Settings::values.use_nvdec_emulation.GetValue());
    AddField(field_type, "Renderer_AccelerateASTC", Settings::values.accelerate_astc.GetValue());
    AddField(field_type, "Renderer_UseVsync", Settings::values.use_vsync.GetValue());
    AddField(field_type, "Renderer_ShaderBackend",
             static_cast<u32>(Settings::values.shader_backend.GetValue()));
    AddField(field_type, "Renderer_UseAsynchronousShaders",
             Settings::values.use_asynchronous_shaders.GetValue());
    AddField(field_type, "System_UseDockedMode", Settings::values.use_docked_mode.GetValue());
}

bool TelemetrySession::SubmitTestcase() {
#ifdef ENABLE_WEB_SERVICE
    auto backend = std::make_unique<WebService::TelemetryJson>(
        Settings::values.web_api_url.GetValue(), Settings::values.yuzu_username.GetValue(),
        Settings::values.yuzu_token.GetValue());
    field_collection.Accept(*backend);
    return backend->SubmitTestcase();
#else
    return false;
#endif
}

} // namespace Core
