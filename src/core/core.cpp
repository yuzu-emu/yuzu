// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <atomic>
#include <memory>
#include <utility>

#include "common/fs/fs.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "core/arm/exclusive_monitor.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/cpu_manager.h"
#include "core/device_memory.h"
#include "core/file_sys/bis_factory.h"
#include "core/file_sys/card_image.h"
#include "core/file_sys/mode.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/romfs_factory.h"
#include "core/file_sys/savedata_factory.h"
#include "core/file_sys/sdmc_factory.h"
#include "core/file_sys/vfs_concat.h"
#include "core/file_sys/vfs_real.h"
#include "core/hardware_interrupt_manager.h"
#include "core/hle/kernel/k_client_port.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/physical_core.h"
#include "core/hle/service/am/applets/applets.h"
#include "core/hle/service/apm/apm_controller.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/glue/glue_manager.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/time/time_manager.h"
#include "core/loader/loader.h"
#include "core/memory.h"
#include "core/memory/cheat_engine.h"
#include "core/network/network.h"
#include "core/perf_stats.h"
#include "core/reporter.h"
#include "core/telemetry_session.h"
#include "core/tools/freezer.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

MICROPROFILE_DEFINE(ARM_Jit_Dynarmic_CPU0, "ARM JIT", "Dynarmic CPU 0", MP_RGB(255, 64, 64));
MICROPROFILE_DEFINE(ARM_Jit_Dynarmic_CPU1, "ARM JIT", "Dynarmic CPU 1", MP_RGB(255, 64, 64));
MICROPROFILE_DEFINE(ARM_Jit_Dynarmic_CPU2, "ARM JIT", "Dynarmic CPU 2", MP_RGB(255, 64, 64));
MICROPROFILE_DEFINE(ARM_Jit_Dynarmic_CPU3, "ARM JIT", "Dynarmic CPU 3", MP_RGB(255, 64, 64));

namespace Core {

namespace {

FileSys::StorageId GetStorageIdForFrontendSlot(
    std::optional<FileSys::ContentProviderUnionSlot> slot) {
    if (!slot.has_value()) {
        return FileSys::StorageId::None;
    }

    switch (*slot) {
    case FileSys::ContentProviderUnionSlot::UserNAND:
        return FileSys::StorageId::NandUser;
    case FileSys::ContentProviderUnionSlot::SysNAND:
        return FileSys::StorageId::NandSystem;
    case FileSys::ContentProviderUnionSlot::SDMC:
        return FileSys::StorageId::SdCard;
    case FileSys::ContentProviderUnionSlot::FrontendManual:
        return FileSys::StorageId::Host;
    default:
        return FileSys::StorageId::None;
    }
}

} // Anonymous namespace

/*static*/ System System::s_instance;

FileSys::VirtualFile GetGameFileFromPath(const FileSys::VirtualFilesystem& vfs,
                                         const std::string& path) {
    // To account for split 00+01+etc files.
    std::string dir_name;
    std::string filename;
    Common::SplitPath(path, &dir_name, &filename, nullptr);

    if (filename == "00") {
        const auto dir = vfs->OpenDirectory(dir_name, FileSys::Mode::Read);
        std::vector<FileSys::VirtualFile> concat;

        for (u32 i = 0; i < 0x10; ++i) {
            const auto file_name = fmt::format("{:02X}", i);
            auto next = dir->GetFile(file_name);

            if (next != nullptr) {
                concat.push_back(std::move(next));
            } else {
                next = dir->GetFile(file_name);

                if (next == nullptr) {
                    break;
                }

                concat.push_back(std::move(next));
            }
        }

        if (concat.empty()) {
            return nullptr;
        }

        return FileSys::ConcatenatedVfsFile::MakeConcatenatedFile(std::move(concat),
                                                                  dir->GetName());
    }

    if (Common::FS::IsDir(path)) {
        return vfs->OpenFile(path + "/main", FileSys::Mode::Read);
    }

    return vfs->OpenFile(path, FileSys::Mode::Read);
}

struct System::Impl {
    explicit Impl(System& system)
        : kernel{system}, fs_controller{system}, memory{system},
          cpu_manager{system}, reporter{system}, applet_manager{system}, time_manager{system} {}

    ResultStatus Run() {
        status = ResultStatus::Success;

        kernel.Suspend(false);
        core_timing.SyncPause(false);
        cpu_manager.Pause(false);

        return status;
    }

    ResultStatus Pause() {
        status = ResultStatus::Success;

        core_timing.SyncPause(true);
        kernel.Suspend(true);
        cpu_manager.Pause(true);

        return status;
    }

    ResultStatus Init(System& system, Frontend::EmuWindow& emu_window) {
        LOG_DEBUG(Core, "initialized OK");

        device_memory = std::make_unique<Core::DeviceMemory>();

        is_multicore = Settings::values.use_multi_core.GetValue();
        is_async_gpu = Settings::values.use_asynchronous_gpu_emulation.GetValue();

        kernel.SetMulticore(is_multicore);
        cpu_manager.SetMulticore(is_multicore);
        cpu_manager.SetAsyncGpu(is_async_gpu);
        core_timing.SetMulticore(is_multicore);

        kernel.Initialize();
        cpu_manager.Initialize();
        core_timing.Initialize([&system]() { system.RegisterHostThread(); });

        const auto current_time = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch());
        Settings::values.custom_rtc_differential =
            Settings::values.custom_rtc.value_or(current_time) - current_time;

        // Create a default fs if one doesn't already exist.
        if (virtual_filesystem == nullptr)
            virtual_filesystem = std::make_shared<FileSys::RealVfsFilesystem>();
        if (content_provider == nullptr)
            content_provider = std::make_unique<FileSys::ContentProviderUnion>();

        /// Create default implementations of applets if one is not provided.
        applet_manager.SetDefaultAppletsIfMissing();

        /// Reset all glue registrations
        arp_manager.ResetAll();

        telemetry_session = std::make_unique<Core::TelemetrySession>();

        gpu_core = VideoCore::CreateGPU(emu_window, system);
        if (!gpu_core) {
            return ResultStatus::ErrorVideoCore;
        }

        service_manager = std::make_shared<Service::SM::ServiceManager>(kernel);
        services = std::make_unique<Service::Services>(service_manager, system);
        interrupt_manager = std::make_unique<Hardware::InterruptManager>(system);

        // Initialize time manager, which must happen after kernel is created
        time_manager.Initialize();

        is_powered_on = true;
        exit_lock = false;

        microprofile_dynarmic[0] = MICROPROFILE_TOKEN(ARM_Jit_Dynarmic_CPU0);
        microprofile_dynarmic[1] = MICROPROFILE_TOKEN(ARM_Jit_Dynarmic_CPU1);
        microprofile_dynarmic[2] = MICROPROFILE_TOKEN(ARM_Jit_Dynarmic_CPU2);
        microprofile_dynarmic[3] = MICROPROFILE_TOKEN(ARM_Jit_Dynarmic_CPU3);

        LOG_DEBUG(Core, "Initialized OK");

        return ResultStatus::Success;
    }

    ResultStatus Load(System& system, Frontend::EmuWindow& emu_window, const std::string& filepath,
                      u64 program_id, std::size_t program_index) {
        app_loader = Loader::GetLoader(system, GetGameFileFromPath(virtual_filesystem, filepath),
                                       program_id, program_index);

        if (!app_loader) {
            LOG_CRITICAL(Core, "Failed to obtain loader for {}!", filepath);
            return ResultStatus::ErrorGetLoader;
        }

        ResultStatus init_result{Init(system, emu_window)};
        if (init_result != ResultStatus::Success) {
            LOG_CRITICAL(Core, "Failed to initialize system (Error {})!",
                         static_cast<int>(init_result));
            Shutdown();
            return init_result;
        }

        telemetry_session->AddInitialInfo(*app_loader, fs_controller, *content_provider);
        auto main_process = Kernel::KProcess::Create(system.Kernel());
        ASSERT(Kernel::KProcess::Initialize(main_process, system, "main",
                                            Kernel::KProcess::ProcessType::Userland)
                   .IsSuccess());
        main_process->Open();
        const auto [load_result, load_parameters] = app_loader->Load(*main_process, system);
        if (load_result != Loader::ResultStatus::Success) {
            LOG_CRITICAL(Core, "Failed to load ROM (Error {})!", load_result);
            Shutdown();

            return static_cast<ResultStatus>(static_cast<u32>(ResultStatus::ErrorLoader) +
                                             static_cast<u32>(load_result));
        }
        AddGlueRegistrationForProcess(*app_loader, *main_process);
        kernel.MakeCurrentProcess(main_process);
        kernel.InitializeCores();

        // Initialize cheat engine
        if (cheat_engine) {
            cheat_engine->Initialize();
        }

        // All threads are started, begin main process execution, now that we're in the clear.
        main_process->Run(load_parameters->main_thread_priority,
                          load_parameters->main_thread_stack_size);

        if (Settings::values.gamecard_inserted) {
            if (Settings::values.gamecard_current_game) {
                fs_controller.SetGameCard(GetGameFileFromPath(virtual_filesystem, filepath));
            } else if (!Settings::values.gamecard_path.GetValue().empty()) {
                const auto gamecard_path = Settings::values.gamecard_path.GetValue();
                fs_controller.SetGameCard(GetGameFileFromPath(virtual_filesystem, gamecard_path));
            }
        }

        if (app_loader->ReadProgramId(program_id) != Loader::ResultStatus::Success) {
            LOG_ERROR(Core, "Failed to find title id for ROM (Error {})", load_result);
        }
        perf_stats = std::make_unique<PerfStats>(program_id);
        // Reset counters and set time origin to current frame
        GetAndResetPerfStats();
        perf_stats->BeginSystemFrame();

        status = ResultStatus::Success;
        return status;
    }

    void Shutdown() {
        // Log last frame performance stats if game was loded
        if (perf_stats) {
            const auto perf_results = GetAndResetPerfStats();
            constexpr auto performance = Common::Telemetry::FieldType::Performance;

            telemetry_session->AddField(performance, "Shutdown_EmulationSpeed",
                                        perf_results.emulation_speed * 100.0);
            telemetry_session->AddField(performance, "Shutdown_Framerate",
                                        perf_results.average_game_fps);
            telemetry_session->AddField(performance, "Shutdown_Frametime",
                                        perf_results.frametime * 1000.0);
            telemetry_session->AddField(performance, "Mean_Frametime_MS",
                                        perf_stats->GetMeanFrametime());
        }

        is_powered_on = false;
        exit_lock = false;

        if (gpu_core) {
            gpu_core->ShutDown();
        }

        services.reset();
        service_manager.reset();
        cheat_engine.reset();
        telemetry_session.reset();
        cpu_manager.Shutdown();
        time_manager.Shutdown();
        core_timing.Shutdown();
        app_loader.reset();
        gpu_core.reset();
        perf_stats.reset();
        kernel.Shutdown();
        memory.Reset();
        applet_manager.ClearAll();

        LOG_DEBUG(Core, "Shutdown OK");
    }

    Loader::ResultStatus GetGameName(std::string& out) const {
        if (app_loader == nullptr)
            return Loader::ResultStatus::ErrorNotInitialized;
        return app_loader->ReadTitle(out);
    }

    void AddGlueRegistrationForProcess(Loader::AppLoader& loader, Kernel::KProcess& process) {
        std::vector<u8> nacp_data;
        FileSys::NACP nacp;
        if (loader.ReadControlData(nacp) == Loader::ResultStatus::Success) {
            nacp_data = nacp.GetRawBytes();
        } else {
            nacp_data.resize(sizeof(FileSys::RawNACP));
        }

        Service::Glue::ApplicationLaunchProperty launch{};
        launch.title_id = process.GetTitleID();

        FileSys::PatchManager pm{launch.title_id, fs_controller, *content_provider};
        launch.version = pm.GetGameVersion().value_or(0);

        // TODO(DarkLordZach): When FSController/Game Card Support is added, if
        // current_process_game_card use correct StorageId
        launch.base_game_storage_id = GetStorageIdForFrontendSlot(content_provider->GetSlotForEntry(
            launch.title_id, FileSys::ContentRecordType::Program));
        launch.update_storage_id = GetStorageIdForFrontendSlot(content_provider->GetSlotForEntry(
            FileSys::GetUpdateTitleID(launch.title_id), FileSys::ContentRecordType::Program));

        arp_manager.Register(launch.title_id, launch, std::move(nacp_data));
    }

    void SetStatus(ResultStatus new_status, const char* details = nullptr) {
        status = new_status;
        if (details) {
            status_details = details;
        }
    }

    PerfStatsResults GetAndResetPerfStats() {
        return perf_stats->GetAndResetStats(core_timing.GetGlobalTimeUs());
    }

    Timing::CoreTiming core_timing;
    Kernel::KernelCore kernel;
    /// RealVfsFilesystem instance
    FileSys::VirtualFilesystem virtual_filesystem;
    /// ContentProviderUnion instance
    std::unique_ptr<FileSys::ContentProviderUnion> content_provider;
    Service::FileSystem::FileSystemController fs_controller;
    /// AppLoader used to load the current executing application
    std::unique_ptr<Loader::AppLoader> app_loader;
    std::unique_ptr<Tegra::GPU> gpu_core;
    std::unique_ptr<Hardware::InterruptManager> interrupt_manager;
    std::unique_ptr<Core::DeviceMemory> device_memory;
    Core::Memory::Memory memory;
    CpuManager cpu_manager;
    std::atomic_bool is_powered_on{};
    bool exit_lock = false;

    Reporter reporter;
    std::unique_ptr<Memory::CheatEngine> cheat_engine;
    std::unique_ptr<Tools::Freezer> memory_freezer;
    std::array<u8, 0x20> build_id{};

    /// Frontend applets
    Service::AM::Applets::AppletManager applet_manager;

    /// APM (Performance) services
    Service::APM::Controller apm_controller{core_timing};

    /// Service State
    Service::Glue::ARPManager arp_manager;
    Service::Time::TimeManager time_manager;

    /// Service manager
    std::shared_ptr<Service::SM::ServiceManager> service_manager;

    /// Services
    std::unique_ptr<Service::Services> services;

    /// Telemetry session for this emulation session
    std::unique_ptr<Core::TelemetrySession> telemetry_session;

    /// Network instance
    Network::NetworkInstance network_instance;

    ResultStatus status = ResultStatus::Success;
    std::string status_details = "";

    std::unique_ptr<Core::PerfStats> perf_stats;
    Core::FrameLimiter frame_limiter;

    bool is_multicore{};
    bool is_async_gpu{};

    ExecuteProgramCallback execute_program_callback;

    std::array<u64, Core::Hardware::NUM_CPU_CORES> dynarmic_ticks{};
    std::array<MicroProfileToken, Core::Hardware::NUM_CPU_CORES> microprofile_dynarmic{};
};

System::System() : impl{std::make_unique<Impl>(*this)} {}
System::~System() = default;

CpuManager& System::GetCpuManager() {
    return impl->cpu_manager;
}

const CpuManager& System::GetCpuManager() const {
    return impl->cpu_manager;
}

System::ResultStatus System::Run() {
    return impl->Run();
}

System::ResultStatus System::Pause() {
    return impl->Pause();
}

System::ResultStatus System::SingleStep() {
    return ResultStatus::Success;
}

void System::InvalidateCpuInstructionCaches() {
    impl->kernel.InvalidateAllInstructionCaches();
}

void System::InvalidateCpuInstructionCacheRange(VAddr addr, std::size_t size) {
    impl->kernel.InvalidateCpuInstructionCacheRange(addr, size);
}

void System::Shutdown() {
    impl->Shutdown();
}

System::ResultStatus System::Load(Frontend::EmuWindow& emu_window, const std::string& filepath,
                                  u64 program_id, std::size_t program_index) {
    return impl->Load(*this, emu_window, filepath, program_id, program_index);
}

bool System::IsPoweredOn() const {
    return impl->is_powered_on.load(std::memory_order::relaxed);
}

void System::PrepareReschedule() {
    // Deprecated, does nothing, kept for backward compatibility.
}

void System::PrepareReschedule(const u32 core_index) {
    impl->kernel.PrepareReschedule(core_index);
}

PerfStatsResults System::GetAndResetPerfStats() {
    return impl->GetAndResetPerfStats();
}

TelemetrySession& System::TelemetrySession() {
    return *impl->telemetry_session;
}

const TelemetrySession& System::TelemetrySession() const {
    return *impl->telemetry_session;
}

ARM_Interface& System::CurrentArmInterface() {
    return impl->kernel.CurrentPhysicalCore().ArmInterface();
}

const ARM_Interface& System::CurrentArmInterface() const {
    return impl->kernel.CurrentPhysicalCore().ArmInterface();
}

std::size_t System::CurrentCoreIndex() const {
    std::size_t core = impl->kernel.GetCurrentHostThreadID();
    ASSERT(core < Core::Hardware::NUM_CPU_CORES);
    return core;
}

Kernel::PhysicalCore& System::CurrentPhysicalCore() {
    return impl->kernel.CurrentPhysicalCore();
}

const Kernel::PhysicalCore& System::CurrentPhysicalCore() const {
    return impl->kernel.CurrentPhysicalCore();
}

/// Gets the global scheduler
Kernel::GlobalSchedulerContext& System::GlobalSchedulerContext() {
    return impl->kernel.GlobalSchedulerContext();
}

/// Gets the global scheduler
const Kernel::GlobalSchedulerContext& System::GlobalSchedulerContext() const {
    return impl->kernel.GlobalSchedulerContext();
}

Kernel::KProcess* System::CurrentProcess() {
    return impl->kernel.CurrentProcess();
}

Core::DeviceMemory& System::DeviceMemory() {
    return *impl->device_memory;
}

const Core::DeviceMemory& System::DeviceMemory() const {
    return *impl->device_memory;
}

const Kernel::KProcess* System::CurrentProcess() const {
    return impl->kernel.CurrentProcess();
}

ARM_Interface& System::ArmInterface(std::size_t core_index) {
    return impl->kernel.PhysicalCore(core_index).ArmInterface();
}

const ARM_Interface& System::ArmInterface(std::size_t core_index) const {
    return impl->kernel.PhysicalCore(core_index).ArmInterface();
}

ExclusiveMonitor& System::Monitor() {
    return impl->kernel.GetExclusiveMonitor();
}

const ExclusiveMonitor& System::Monitor() const {
    return impl->kernel.GetExclusiveMonitor();
}

Memory::Memory& System::Memory() {
    return impl->memory;
}

const Core::Memory::Memory& System::Memory() const {
    return impl->memory;
}

Tegra::GPU& System::GPU() {
    return *impl->gpu_core;
}

const Tegra::GPU& System::GPU() const {
    return *impl->gpu_core;
}

Core::Hardware::InterruptManager& System::InterruptManager() {
    return *impl->interrupt_manager;
}

const Core::Hardware::InterruptManager& System::InterruptManager() const {
    return *impl->interrupt_manager;
}

VideoCore::RendererBase& System::Renderer() {
    return impl->gpu_core->Renderer();
}

const VideoCore::RendererBase& System::Renderer() const {
    return impl->gpu_core->Renderer();
}

Kernel::KernelCore& System::Kernel() {
    return impl->kernel;
}

const Kernel::KernelCore& System::Kernel() const {
    return impl->kernel;
}

Timing::CoreTiming& System::CoreTiming() {
    return impl->core_timing;
}

const Timing::CoreTiming& System::CoreTiming() const {
    return impl->core_timing;
}

Core::PerfStats& System::GetPerfStats() {
    return *impl->perf_stats;
}

const Core::PerfStats& System::GetPerfStats() const {
    return *impl->perf_stats;
}

Core::FrameLimiter& System::FrameLimiter() {
    return impl->frame_limiter;
}

const Core::FrameLimiter& System::FrameLimiter() const {
    return impl->frame_limiter;
}

Loader::ResultStatus System::GetGameName(std::string& out) const {
    return impl->GetGameName(out);
}

void System::SetStatus(ResultStatus new_status, const char* details) {
    impl->SetStatus(new_status, details);
}

const std::string& System::GetStatusDetails() const {
    return impl->status_details;
}

Loader::AppLoader& System::GetAppLoader() {
    return *impl->app_loader;
}

const Loader::AppLoader& System::GetAppLoader() const {
    return *impl->app_loader;
}

void System::SetFilesystem(FileSys::VirtualFilesystem vfs) {
    impl->virtual_filesystem = std::move(vfs);
}

FileSys::VirtualFilesystem System::GetFilesystem() const {
    return impl->virtual_filesystem;
}

void System::RegisterCheatList(const std::vector<Memory::CheatEntry>& list,
                               const std::array<u8, 32>& build_id, VAddr main_region_begin,
                               u64 main_region_size) {
    impl->cheat_engine = std::make_unique<Memory::CheatEngine>(*this, list, build_id);
    impl->cheat_engine->SetMainMemoryParameters(main_region_begin, main_region_size);
}

void System::SetAppletFrontendSet(Service::AM::Applets::AppletFrontendSet&& set) {
    impl->applet_manager.SetAppletFrontendSet(std::move(set));
}

void System::SetDefaultAppletFrontendSet() {
    impl->applet_manager.SetDefaultAppletFrontendSet();
}

Service::AM::Applets::AppletManager& System::GetAppletManager() {
    return impl->applet_manager;
}

const Service::AM::Applets::AppletManager& System::GetAppletManager() const {
    return impl->applet_manager;
}

void System::SetContentProvider(std::unique_ptr<FileSys::ContentProviderUnion> provider) {
    impl->content_provider = std::move(provider);
}

FileSys::ContentProvider& System::GetContentProvider() {
    return *impl->content_provider;
}

const FileSys::ContentProvider& System::GetContentProvider() const {
    return *impl->content_provider;
}

Service::FileSystem::FileSystemController& System::GetFileSystemController() {
    return impl->fs_controller;
}

const Service::FileSystem::FileSystemController& System::GetFileSystemController() const {
    return impl->fs_controller;
}

void System::RegisterContentProvider(FileSys::ContentProviderUnionSlot slot,
                                     FileSys::ContentProvider* provider) {
    impl->content_provider->SetSlot(slot, provider);
}

void System::ClearContentProvider(FileSys::ContentProviderUnionSlot slot) {
    impl->content_provider->ClearSlot(slot);
}

const Reporter& System::GetReporter() const {
    return impl->reporter;
}

Service::Glue::ARPManager& System::GetARPManager() {
    return impl->arp_manager;
}

const Service::Glue::ARPManager& System::GetARPManager() const {
    return impl->arp_manager;
}

Service::APM::Controller& System::GetAPMController() {
    return impl->apm_controller;
}

const Service::APM::Controller& System::GetAPMController() const {
    return impl->apm_controller;
}

Service::Time::TimeManager& System::GetTimeManager() {
    return impl->time_manager;
}

const Service::Time::TimeManager& System::GetTimeManager() const {
    return impl->time_manager;
}

void System::SetExitLock(bool locked) {
    impl->exit_lock = locked;
}

bool System::GetExitLock() const {
    return impl->exit_lock;
}

void System::SetCurrentProcessBuildID(const CurrentBuildProcessID& id) {
    impl->build_id = id;
}

const System::CurrentBuildProcessID& System::GetCurrentProcessBuildID() const {
    return impl->build_id;
}

Service::SM::ServiceManager& System::ServiceManager() {
    return *impl->service_manager;
}

const Service::SM::ServiceManager& System::ServiceManager() const {
    return *impl->service_manager;
}

void System::RegisterCoreThread(std::size_t id) {
    impl->kernel.RegisterCoreThread(id);
}

void System::RegisterHostThread() {
    impl->kernel.RegisterHostThread();
}

void System::EnterDynarmicProfile() {
    std::size_t core = impl->kernel.GetCurrentHostThreadID();
    impl->dynarmic_ticks[core] = MicroProfileEnter(impl->microprofile_dynarmic[core]);
}

void System::ExitDynarmicProfile() {
    std::size_t core = impl->kernel.GetCurrentHostThreadID();
    MicroProfileLeave(impl->microprofile_dynarmic[core], impl->dynarmic_ticks[core]);
}

bool System::IsMulticore() const {
    return impl->is_multicore;
}

void System::RegisterExecuteProgramCallback(ExecuteProgramCallback&& callback) {
    impl->execute_program_callback = std::move(callback);
}

void System::ExecuteProgram(std::size_t program_index) {
    if (impl->execute_program_callback) {
        impl->execute_program_callback(program_index);
    } else {
        LOG_CRITICAL(Core, "execute_program_callback must be initialized by the frontend");
    }
}

void System::ApplySettings() {
    if (IsPoweredOn()) {
        Renderer().RefreshBaseSettings();
    }

    Service::HID::ReloadInputDevices();
}

} // namespace Core
