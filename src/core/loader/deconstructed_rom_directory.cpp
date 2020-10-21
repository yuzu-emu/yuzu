// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cinttypes>
#include <cstring>
#include "common/common_funcs.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/romfs_factory.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/memory/page_table.h"
#include "core/hle/kernel/process.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/loader/deconstructed_rom_directory.h"
#include "core/loader/nso.h"

namespace Loader {

AppLoader_DeconstructedRomDirectory::AppLoader_DeconstructedRomDirectory(FileSys::VirtualFile file_,
                                                                         bool override_update)
    : AppLoader(std::move(file_)), override_update(override_update) {
    const auto dir = file->GetContainingDirectory();

    // Title ID
    const auto npdm = dir->GetFile("main.npdm");
    if (npdm != nullptr) {
        const auto res = metadata.Load(npdm);
        if (res == ResultStatus::Success)
            title_id = metadata.GetTitleID();
    }

    // Icon
    FileSys::VirtualFile icon_file = nullptr;
    for (const auto& language : FileSys::LANGUAGE_NAMES) {
        icon_file = dir->GetFile("icon_" + std::string(language) + ".dat");
        if (icon_file != nullptr) {
            icon_data = icon_file->ReadAllBytes();
            break;
        }
    }

    if (icon_data.empty()) {
        // Any png, jpeg, or bmp file
        const auto& files = dir->GetFiles();
        const auto icon_iter =
            std::find_if(files.begin(), files.end(), [](const FileSys::VirtualFile& file) {
                return file->GetExtension() == "png" || file->GetExtension() == "jpg" ||
                       file->GetExtension() == "bmp" || file->GetExtension() == "jpeg";
            });
        if (icon_iter != files.end())
            icon_data = (*icon_iter)->ReadAllBytes();
    }

    // Metadata
    FileSys::VirtualFile nacp_file = dir->GetFile("control.nacp");
    if (nacp_file == nullptr) {
        const auto& files = dir->GetFiles();
        const auto nacp_iter =
            std::find_if(files.begin(), files.end(), [](const FileSys::VirtualFile& file) {
                return file->GetExtension() == "nacp";
            });
        if (nacp_iter != files.end())
            nacp_file = *nacp_iter;
    }

    if (nacp_file != nullptr) {
        FileSys::NACP nacp(nacp_file);
        name = nacp.GetApplicationName();
    }
}

AppLoader_DeconstructedRomDirectory::AppLoader_DeconstructedRomDirectory(
    FileSys::VirtualDir directory, bool override_update)
    : AppLoader(directory->GetFile("main")), dir(std::move(directory)),
      override_update(override_update) {}

FileType AppLoader_DeconstructedRomDirectory::IdentifyType(const FileSys::VirtualFile& file) {
    if (FileSys::IsDirectoryExeFS(file->GetContainingDirectory())) {
        return FileType::DeconstructedRomDirectory;
    }

    return FileType::Error;
}

AppLoader_DeconstructedRomDirectory::LoadResult AppLoader_DeconstructedRomDirectory::Load(
    Kernel::Process& process, Core::System& system) {
    if (is_loaded) {
        return {ResultStatus::ErrorAlreadyLoaded, {}};
    }

    if (dir == nullptr) {
        if (file == nullptr) {
            return {ResultStatus::ErrorNullFile, {}};
        }

        dir = file->GetContainingDirectory();
    }

    // Read meta to determine title ID
    FileSys::VirtualFile npdm = dir->GetFile("main.npdm");
    if (npdm == nullptr) {
        return {ResultStatus::ErrorMissingNPDM, {}};
    }

    const ResultStatus result = metadata.Load(npdm);
    if (result != ResultStatus::Success) {
        return {result, {}};
    }

    if (override_update) {
        const FileSys::PatchManager patch_manager(metadata.GetTitleID());
        dir = patch_manager.PatchExeFS(dir);
    }

    // Reread in case PatchExeFS affected the main.npdm
    npdm = dir->GetFile("main.npdm");
    if (npdm == nullptr) {
        return {ResultStatus::ErrorMissingNPDM, {}};
    }

    const ResultStatus result2 = metadata.Load(npdm);
    if (result2 != ResultStatus::Success) {
        return {result2, {}};
    }
    metadata.Print();

    const auto static_modules = {"rtld",    "main",    "subsdk0", "subsdk1", "subsdk2", "subsdk3",
                                 "subsdk4", "subsdk5", "subsdk6", "subsdk7", "sdk"};

    // Use the NSO module loader to figure out the code layout
    std::size_t code_size{};
    for (const auto& mod : static_modules) {
        const FileSys::VirtualFile module_file{dir->GetFile(mod)};
        if (!module_file) {
            continue;
        }

        const bool should_pass_arguments = std::strcmp(mod, "rtld") == 0;
        const auto tentative_next_load_addr = AppLoader_NSO::LoadModule(
            process, system, *module_file, code_size, should_pass_arguments, false);
        if (!tentative_next_load_addr) {
            return {ResultStatus::ErrorLoadingNSO, {}};
        }

        code_size = *tentative_next_load_addr;
    }

    // Setup the process code layout
    if (process.LoadFromMetadata(metadata, code_size).IsError()) {
        return {ResultStatus::ErrorUnableToParseKernelMetadata, {}};
    }

    // Load NSO modules
    modules.clear();
    const VAddr base_address{process.PageTable().GetCodeRegionStart()};
    VAddr next_load_addr{base_address};
    const FileSys::PatchManager pm{metadata.GetTitleID()};
    for (const auto& mod : static_modules) {
        const FileSys::VirtualFile module_file{dir->GetFile(mod)};
        if (!module_file) {
            continue;
        }

        const VAddr load_addr{next_load_addr};
        const bool should_pass_arguments = std::strcmp(mod, "rtld") == 0;
        const auto tentative_next_load_addr = AppLoader_NSO::LoadModule(
            process, system, *module_file, load_addr, should_pass_arguments, true, pm);
        if (!tentative_next_load_addr) {
            return {ResultStatus::ErrorLoadingNSO, {}};
        }

        next_load_addr = *tentative_next_load_addr;
        modules.insert_or_assign(load_addr, mod);
        LOG_DEBUG(Loader, "loaded module {} @ 0x{:X}", mod, load_addr);
        // Register module with GDBStub
        GDBStub::RegisterModule(mod, load_addr, next_load_addr - 1, false);
    }

    // Find the RomFS by searching for a ".romfs" file in this directory
    const auto& files = dir->GetFiles();
    const auto romfs_iter =
        std::find_if(files.begin(), files.end(), [](const FileSys::VirtualFile& file) {
            return file->GetName().find(".romfs") != std::string::npos;
        });

    // Register the RomFS if a ".romfs" file was found
    if (romfs_iter != files.end() && *romfs_iter != nullptr) {
        romfs = *romfs_iter;
        system.GetFileSystemController().RegisterRomFS(std::make_unique<FileSys::RomFSFactory>(
            *this, system.GetContentProvider(), system.GetFileSystemController()));
    }

    is_loaded = true;
    return {ResultStatus::Success,
            LoadParameters{metadata.GetMainThreadPriority(), metadata.GetMainThreadStackSize()}};
}

ResultStatus AppLoader_DeconstructedRomDirectory::ReadRomFS(FileSys::VirtualFile& dir) {
    if (romfs == nullptr)
        return ResultStatus::ErrorNoRomFS;
    dir = romfs;
    return ResultStatus::Success;
}

ResultStatus AppLoader_DeconstructedRomDirectory::ReadIcon(std::vector<u8>& buffer) {
    if (icon_data.empty())
        return ResultStatus::ErrorNoIcon;
    buffer = icon_data;
    return ResultStatus::Success;
}

ResultStatus AppLoader_DeconstructedRomDirectory::ReadProgramId(u64& out_program_id) {
    out_program_id = title_id;
    return ResultStatus::Success;
}

ResultStatus AppLoader_DeconstructedRomDirectory::ReadTitle(std::string& title) {
    if (name.empty())
        return ResultStatus::ErrorNoControl;
    title = name;
    return ResultStatus::Success;
}

bool AppLoader_DeconstructedRomDirectory::IsRomFSUpdatable() const {
    return false;
}

ResultStatus AppLoader_DeconstructedRomDirectory::ReadNSOModules(Modules& modules) {
    if (!is_loaded) {
        return ResultStatus::ErrorNotInitialized;
    }

    modules = this->modules;
    return ResultStatus::Success;
}

} // namespace Loader
