// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <chrono>

#include "common/common_types.h"

namespace Common::Log {

/// Specifies the severity or level of detail of the log message.
enum class Level : u8 {
    Trace,    ///< Extremely detailed and repetitive debugging information that is likely to
              ///< pollute logs.
    Debug,    ///< Less detailed debugging information.
    Info,     ///< Status information from important points during execution.
    Warning,  ///< Minor or potential problems found during execution of a task.
    Error,    ///< Major problems found during execution of a task that prevent it from being
              ///< completed.
    Critical, ///< Major problems during execution that threaten the stability of the entire
              ///< application.

    Count ///< Total number of logging levels
};

/**
 * Specifies the sub-system that generated the log message.
 *
 * @note If you add a new entry here, also add a corresponding one to `ALL_LOG_CLASSES` in
 * filter.cpp.
 */
enum class Class : u8 {
    Log,               ///< Messages about the log system itself
    Common,            ///< Library routines
    Common_Filesystem, ///< Filesystem interface library
    Common_Memory,     ///< Memory mapping and management functions
    Core,              ///< LLE emulation core
    Core_ARM,          ///< ARM CPU core
    Core_Timing,       ///< CoreTiming functions
    Config,            ///< Emulator configuration (including commandline)
    Debug,             ///< Debugging tools
    Debug_Emulated,    ///< Debug messages from the emulated programs
    Debug_GPU,         ///< GPU debugging tools
    Debug_Breakpoint,  ///< Logging breakpoints and watchpoints
    Debug_GDBStub,     ///< GDB Stub
    Kernel,            ///< The HLE implementation of the CTR kernel
    Kernel_SVC,        ///< Kernel system calls
    Service,           ///< HLE implementation of system services. Each major service
                       ///< should have its own subclass.
    Service_ACC,       ///< The ACC (Accounts) service
    Service_AM,        ///< The AM (Applet manager) service
    Service_AOC,       ///< The AOC (AddOn Content) service
    Service_APM,       ///< The APM (Performance) service
    Service_ARP,       ///< The ARP service
    Service_Audio,     ///< The Audio (Audio control) service
    Service_BCAT,      ///< The BCAT service
    Service_BGTC,      ///< The BGTC (Background Task Controller) service
    Service_BPC,       ///< The BPC service
    Service_BTDRV,     ///< The Bluetooth driver service
    Service_BTM,       ///< The BTM service
    Service_Capture,   ///< The capture service
    Service_ERPT,      ///< The error reporting service
    Service_ETicket,   ///< The ETicket service
    Service_EUPLD,     ///< The error upload service
    Service_Fatal,     ///< The Fatal service
    Service_FGM,       ///< The FGM service
    Service_Friend,    ///< The friend service
    Service_FS,        ///< The FS (Filesystem) service
    Service_GRC,       ///< The game recording service
    Service_HID,       ///< The HID (Human interface device) service
    Service_IRS,       ///< The IRS service
    Service_LBL,       ///< The LBL (LCD backlight) service
    Service_LDN,       ///< The LDN (Local domain network) service
    Service_LDR,       ///< The loader service
    Service_LM,        ///< The LM (Logger) service
    Service_Migration, ///< The migration service
    Service_Mii,       ///< The Mii service
    Service_MM,        ///< The MM (Multimedia) service
    Service_NCM,       ///< The NCM service
    Service_NFC,       ///< The NFC (Near-field communication) service
    Service_NFP,       ///< The NFP service
    Service_NIFM,      ///< The NIFM (Network interface) service
    Service_NIM,       ///< The NIM service
    Service_NPNS,      ///< The NPNS service
    Service_NS,        ///< The NS services
    Service_NVDRV,     ///< The NVDRV (Nvidia driver) service
    Service_OLSC,      ///< The OLSC service
    Service_PCIE,      ///< The PCIe service
    Service_PCTL,      ///< The PCTL (Parental control) service
    Service_PCV,       ///< The PCV service
    Service_PM,        ///< The PM service
    Service_PREPO,     ///< The PREPO (Play report) service
    Service_PSC,       ///< The PSC service
    Service_PSM,       ///< The PSM service
    Service_SET,       ///< The SET (Settings) service
    Service_SM,        ///< The SM (Service manager) service
    Service_SPL,       ///< The SPL service
    Service_SSL,       ///< The SSL service
    Service_TCAP,      ///< The TCAP service.
    Service_Time,      ///< The time service
    Service_USB,       ///< The USB (Universal Serial Bus) service
    Service_VI,        ///< The VI (Video interface) service
    Service_WLAN,      ///< The WLAN (Wireless local area network) service
    HW,                ///< Low-level hardware emulation
    HW_Memory,         ///< Memory-map and address translation
    HW_LCD,            ///< LCD register emulation
    HW_GPU,            ///< GPU control emulation
    HW_AES,            ///< AES engine emulation
    IPC,               ///< IPC interface
    Frontend,          ///< Emulator UI
    Render,            ///< Emulator video output and hardware acceleration
    Render_Software,   ///< Software renderer backend
    Render_OpenGL,     ///< OpenGL backend
    Render_Vulkan,     ///< Vulkan backend
    Shader,            ///< Shader recompiler
    Shader_SPIRV,      ///< Shader SPIR-V code generation
    Shader_GLASM,      ///< Shader GLASM code generation
    Shader_GLSL,       ///< Shader GLSL code generation
    Audio,             ///< Audio emulation
    Audio_DSP,         ///< The HLE implementation of the DSP
    Audio_Sink,        ///< Emulator audio output backend
    Loader,            ///< ROM loader
    CheatEngine,       ///< Memory manipulation and engine VM functions
    Crypto,            ///< Cryptographic engine/functions
    Input,             ///< Input emulation
    Network,           ///< Network emulation
    WebService,        ///< Interface to yuzu Web Services
    Count              ///< Total number of logging classes
};

/**
 * A log entry. Log entries are store in a structured format to permit more varied output
 * formatting on different frontends, as well as facilitating filtering and aggregation.
 */
struct Entry {
    std::chrono::microseconds timestamp;
    Class log_class{};
    Level log_level{};
    const char* filename = nullptr;
    unsigned int line_num = 0;
    std::string function;
    std::string message;
    bool final_entry = false;
};

} // namespace Common::Log
