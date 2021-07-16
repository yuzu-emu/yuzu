// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "common/common_types.h"
#include "core/file_sys/vfs_types.h"

namespace Core::Frontend {
class EmuWindow;
} // namespace Core::Frontend

namespace FileSys {
class ContentProvider;
class ContentProviderUnion;
enum class ContentProviderUnionSlot;
class VfsFilesystem;
} // namespace FileSys

namespace Kernel {
class GlobalSchedulerContext;
class KernelCore;
class PhysicalCore;
class KProcess;
class KScheduler;
} // namespace Kernel

namespace Loader {
class AppLoader;
enum class ResultStatus : u16;
} // namespace Loader

namespace Core::Memory {
struct CheatEntry;
class Memory;
} // namespace Core::Memory

namespace Service {

namespace AM::Applets {
struct AppletFrontendSet;
class AppletManager;
} // namespace AM::Applets

namespace APM {
class Controller;
}

namespace FileSystem {
class FileSystemController;
} // namespace FileSystem

namespace Glue {
class ARPManager;
}

namespace SM {
class ServiceManager;
} // namespace SM

namespace Time {
class TimeManager;
} // namespace Time

} // namespace Service

namespace Tegra {
class DebugContext;
class GPU;
} // namespace Tegra

namespace VideoCore {
class RendererBase;
} // namespace VideoCore

namespace Core::Timing {
class CoreTiming;
}

namespace Core::Hardware {
class InterruptManager;
}

namespace Core {

class ARM_Interface;
class CpuManager;
class DeviceMemory;
class ExclusiveMonitor;
class FrameLimiter;
class PerfStats;
class Reporter;
class TelemetrySession;

struct PerfStatsResults;

FileSys::VirtualFile GetGameFileFromPath(const FileSys::VirtualFilesystem& vfs,
                                         const std::string& path);

class System {
public:
    using CurrentBuildProcessID = std::array<u8, 0x20>;

    System(const System&) = delete;
    System& operator=(const System&) = delete;

    System(System&&) = delete;
    System& operator=(System&&) = delete;

    ~System();

    /**
     * Gets the instance of the System singleton class.
     * @returns Reference to the instance of the System singleton class.
     */
    [[deprecated("Use of the global system instance is deprecated")]] static System& GetInstance() {
        return s_instance;
    }

    /// Enumeration representing the return values of the System Initialize and Load process.
    enum class ResultStatus : u32 {
        Success,             ///< Succeeded
        ErrorNotInitialized, ///< Error trying to use core prior to initialization
        ErrorGetLoader,      ///< Error finding the correct application loader
        ErrorSystemFiles,    ///< Error in finding system files
        ErrorSharedFont,     ///< Error in finding shared font
        ErrorVideoCore,      ///< Error in the video core
        ErrorUnknown,        ///< Any other error
        ErrorLoader,         ///< The base for loader errors (too many to repeat)
    };

    /**
     * Run the OS and Application
     * This function will start emulation and run the relevant devices
     */
    [[nodiscard]] ResultStatus Run();

    /**
     * Pause the OS and Application
     * This function will pause emulation and stop the relevant devices
     */
    [[nodiscard]] ResultStatus Pause();

    /**
     * Step the CPU one instruction
     * @return Result status, indicating whether or not the operation succeeded.
     */
    [[nodiscard]] ResultStatus SingleStep();

    /**
     * Invalidate the CPU instruction caches
     * This function should only be used by GDB Stub to support breakpoints, memory updates and
     * step/continue commands.
     */
    void InvalidateCpuInstructionCaches();

    void InvalidateCpuInstructionCacheRange(VAddr addr, std::size_t size);

    /// Shutdown the emulated system.
    void Shutdown();

    /**
     * Load an executable application.
     * @param emu_window Reference to the host-system window used for video output and keyboard
     *                   input.
     * @param filepath String path to the executable application to load on the host file system.
     * @param program_index Specifies the index within the container of the program to launch.
     * @returns ResultStatus code, indicating if the operation succeeded.
     */
    [[nodiscard]] ResultStatus Load(Frontend::EmuWindow& emu_window, const std::string& filepath,
                                    u64 program_id = 0, std::size_t program_index = 0);

    /**
     * Indicates if the emulated system is powered on (all subsystems initialized and able to run an
     * application).
     * @returns True if the emulated system is powered on, otherwise false.
     */
    [[nodiscard]] bool IsPoweredOn() const;

    /// Gets a reference to the telemetry session for this emulation session.
    [[nodiscard]] Core::TelemetrySession& TelemetrySession();

    /// Gets a reference to the telemetry session for this emulation session.
    [[nodiscard]] const Core::TelemetrySession& TelemetrySession() const;

    /// Prepare the core emulation for a reschedule
    void PrepareReschedule();

    /// Prepare the core emulation for a reschedule
    void PrepareReschedule(u32 core_index);

    /// Gets and resets core performance statistics
    [[nodiscard]] PerfStatsResults GetAndResetPerfStats();

    /// Gets an ARM interface to the CPU core that is currently running
    [[nodiscard]] ARM_Interface& CurrentArmInterface();

    /// Gets an ARM interface to the CPU core that is currently running
    [[nodiscard]] const ARM_Interface& CurrentArmInterface() const;

    /// Gets the index of the currently running CPU core
    [[nodiscard]] std::size_t CurrentCoreIndex() const;

    /// Gets the physical core for the CPU core that is currently running
    [[nodiscard]] Kernel::PhysicalCore& CurrentPhysicalCore();

    /// Gets the physical core for the CPU core that is currently running
    [[nodiscard]] const Kernel::PhysicalCore& CurrentPhysicalCore() const;

    /// Gets a reference to an ARM interface for the CPU core with the specified index
    [[nodiscard]] ARM_Interface& ArmInterface(std::size_t core_index);

    /// Gets a const reference to an ARM interface from the CPU core with the specified index
    [[nodiscard]] const ARM_Interface& ArmInterface(std::size_t core_index) const;

    /// Gets a reference to the underlying CPU manager.
    [[nodiscard]] CpuManager& GetCpuManager();

    /// Gets a const reference to the underlying CPU manager
    [[nodiscard]] const CpuManager& GetCpuManager() const;

    /// Gets a reference to the exclusive monitor
    [[nodiscard]] ExclusiveMonitor& Monitor();

    /// Gets a constant reference to the exclusive monitor
    [[nodiscard]] const ExclusiveMonitor& Monitor() const;

    /// Gets a mutable reference to the system memory instance.
    [[nodiscard]] Core::Memory::Memory& Memory();

    /// Gets a constant reference to the system memory instance.
    [[nodiscard]] const Core::Memory::Memory& Memory() const;

    /// Gets a mutable reference to the GPU interface
    [[nodiscard]] Tegra::GPU& GPU();

    /// Gets an immutable reference to the GPU interface.
    [[nodiscard]] const Tegra::GPU& GPU() const;

    /// Gets a mutable reference to the renderer.
    [[nodiscard]] VideoCore::RendererBase& Renderer();

    /// Gets an immutable reference to the renderer.
    [[nodiscard]] const VideoCore::RendererBase& Renderer() const;

    /// Gets the global scheduler
    [[nodiscard]] Kernel::GlobalSchedulerContext& GlobalSchedulerContext();

    /// Gets the global scheduler
    [[nodiscard]] const Kernel::GlobalSchedulerContext& GlobalSchedulerContext() const;

    /// Gets the manager for the guest device memory
    [[nodiscard]] Core::DeviceMemory& DeviceMemory();

    /// Gets the manager for the guest device memory
    [[nodiscard]] const Core::DeviceMemory& DeviceMemory() const;

    /// Provides a pointer to the current process
    [[nodiscard]] Kernel::KProcess* CurrentProcess();

    /// Provides a constant pointer to the current process.
    [[nodiscard]] const Kernel::KProcess* CurrentProcess() const;

    /// Provides a reference to the core timing instance.
    [[nodiscard]] Timing::CoreTiming& CoreTiming();

    /// Provides a constant reference to the core timing instance.
    [[nodiscard]] const Timing::CoreTiming& CoreTiming() const;

    /// Provides a reference to the interrupt manager instance.
    [[nodiscard]] Core::Hardware::InterruptManager& InterruptManager();

    /// Provides a constant reference to the interrupt manager instance.
    [[nodiscard]] const Core::Hardware::InterruptManager& InterruptManager() const;

    /// Provides a reference to the kernel instance.
    [[nodiscard]] Kernel::KernelCore& Kernel();

    /// Provides a constant reference to the kernel instance.
    [[nodiscard]] const Kernel::KernelCore& Kernel() const;

    /// Provides a reference to the internal PerfStats instance.
    [[nodiscard]] Core::PerfStats& GetPerfStats();

    /// Provides a constant reference to the internal PerfStats instance.
    [[nodiscard]] const Core::PerfStats& GetPerfStats() const;

    /// Provides a reference to the frame limiter;
    [[nodiscard]] Core::FrameLimiter& FrameLimiter();

    /// Provides a constant referent to the frame limiter
    [[nodiscard]] const Core::FrameLimiter& FrameLimiter() const;

    /// Gets the name of the current game
    [[nodiscard]] Loader::ResultStatus GetGameName(std::string& out) const;

    void SetStatus(ResultStatus new_status, const char* details);

    [[nodiscard]] const std::string& GetStatusDetails() const;

    [[nodiscard]] Loader::AppLoader& GetAppLoader();
    [[nodiscard]] const Loader::AppLoader& GetAppLoader() const;

    [[nodiscard]] Service::SM::ServiceManager& ServiceManager();
    [[nodiscard]] const Service::SM::ServiceManager& ServiceManager() const;

    void SetFilesystem(FileSys::VirtualFilesystem vfs);

    [[nodiscard]] FileSys::VirtualFilesystem GetFilesystem() const;

    void RegisterCheatList(const std::vector<Memory::CheatEntry>& list,
                           const std::array<u8, 0x20>& build_id, VAddr main_region_begin,
                           u64 main_region_size);

    void SetAppletFrontendSet(Service::AM::Applets::AppletFrontendSet&& set);
    void SetDefaultAppletFrontendSet();

    [[nodiscard]] Service::AM::Applets::AppletManager& GetAppletManager();
    [[nodiscard]] const Service::AM::Applets::AppletManager& GetAppletManager() const;

    void SetContentProvider(std::unique_ptr<FileSys::ContentProviderUnion> provider);

    [[nodiscard]] FileSys::ContentProvider& GetContentProvider();
    [[nodiscard]] const FileSys::ContentProvider& GetContentProvider() const;

    [[nodiscard]] Service::FileSystem::FileSystemController& GetFileSystemController();
    [[nodiscard]] const Service::FileSystem::FileSystemController& GetFileSystemController() const;

    void RegisterContentProvider(FileSys::ContentProviderUnionSlot slot,
                                 FileSys::ContentProvider* provider);

    void ClearContentProvider(FileSys::ContentProviderUnionSlot slot);

    [[nodiscard]] const Reporter& GetReporter() const;

    [[nodiscard]] Service::Glue::ARPManager& GetARPManager();
    [[nodiscard]] const Service::Glue::ARPManager& GetARPManager() const;

    [[nodiscard]] Service::APM::Controller& GetAPMController();
    [[nodiscard]] const Service::APM::Controller& GetAPMController() const;

    [[nodiscard]] Service::Time::TimeManager& GetTimeManager();
    [[nodiscard]] const Service::Time::TimeManager& GetTimeManager() const;

    void SetExitLock(bool locked);
    [[nodiscard]] bool GetExitLock() const;

    void SetCurrentProcessBuildID(const CurrentBuildProcessID& id);
    [[nodiscard]] const CurrentBuildProcessID& GetCurrentProcessBuildID() const;

    /// Register a host thread as an emulated CPU Core.
    void RegisterCoreThread(std::size_t id);

    /// Register a host thread as an auxiliary thread.
    void RegisterHostThread();

    /// Enter Dynarmic Microprofile
    void EnterDynarmicProfile();

    /// Exit Dynarmic Microprofile
    void ExitDynarmicProfile();

    /// Tells if system is running on multicore.
    [[nodiscard]] bool IsMulticore() const;

    /// Type used for the frontend to designate a callback for System to re-launch the application
    /// using a specified program index.
    using ExecuteProgramCallback = std::function<void(std::size_t)>;

    /**
     * Registers a callback from the frontend for System to re-launch the application using a
     * specified program index.
     * @param callback Callback from the frontend to relaunch the application.
     */
    void RegisterExecuteProgramCallback(ExecuteProgramCallback&& callback);

    /**
     * Instructs the frontend to re-launch the application using the specified program_index.
     * @param program_index Specifies the index within the application of the program to launch.
     */
    void ExecuteProgram(std::size_t program_index);

    /// Applies any changes to settings to this core instance.
    void ApplySettings();

private:
    System();

    struct Impl;
    std::unique_ptr<Impl> impl;

    static System s_instance;
};

} // namespace Core
