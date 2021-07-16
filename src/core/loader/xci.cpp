// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <vector>

#include "common/common_types.h"
#include "core/core.h"
#include "core/file_sys/card_image.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/romfs.h"
#include "core/file_sys/submission_package.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/loader/nca.h"
#include "core/loader/xci.h"

namespace Loader {

AppLoader_XCI::AppLoader_XCI(FileSys::VirtualFile file_,
                             const Service::FileSystem::FileSystemController& fsc,
                             const FileSys::ContentProvider& content_provider, u64 program_id,
                             std::size_t program_index)
    : AppLoader(file_), xci(std::make_unique<FileSys::XCI>(file_, program_id, program_index)),
      nca_loader(std::make_unique<AppLoader_NCA>(xci->GetProgramNCAFile())) {
    if (xci->GetStatus() != ResultStatus::Success) {
        return;
    }

    const auto control_nca = xci->GetNCAByType(FileSys::NCAContentType::Control);
    if (control_nca == nullptr || control_nca->GetStatus() != ResultStatus::Success) {
        return;
    }

    std::tie(nacp_file, icon_file) = [this, &content_provider, &control_nca, &fsc] {
        const FileSys::PatchManager pm{xci->GetProgramTitleID(), fsc, content_provider};
        return pm.ParseControlNCA(*control_nca);
    }();
}

AppLoader_XCI::~AppLoader_XCI() = default;

FileType AppLoader_XCI::IdentifyType(const FileSys::VirtualFile& xci_file) {
    const FileSys::XCI xci(xci_file);

    if (xci.GetStatus() == ResultStatus::Success &&
        xci.GetNCAByType(FileSys::NCAContentType::Program) != nullptr &&
        AppLoader_NCA::IdentifyType(xci.GetNCAFileByType(FileSys::NCAContentType::Program)) ==
            FileType::NCA) {
        return FileType::XCI;
    }

    return FileType::Error;
}

AppLoader_XCI::LoadResult AppLoader_XCI::Load(Kernel::KProcess& process, Core::System& system) {
    if (is_loaded) {
        return {ResultStatus::ErrorAlreadyLoaded, {}};
    }

    if (xci->GetStatus() != ResultStatus::Success) {
        return {xci->GetStatus(), {}};
    }

    if (xci->GetProgramNCAStatus() != ResultStatus::Success) {
        return {xci->GetProgramNCAStatus(), {}};
    }

    if (!xci->HasProgramNCA() && !Core::Crypto::KeyManager::KeyFileExists(false)) {
        return {ResultStatus::ErrorMissingProductionKeyFile, {}};
    }

    const auto result = nca_loader->Load(process, system);
    if (result.first != ResultStatus::Success) {
        return result;
    }

    FileSys::VirtualFile update_raw;
    if (ReadUpdateRaw(update_raw) == ResultStatus::Success && update_raw != nullptr) {
        system.GetFileSystemController().SetPackedUpdate(std::move(update_raw));
    }

    is_loaded = true;
    return result;
}

ResultStatus AppLoader_XCI::ReadRomFS(FileSys::VirtualFile& out_file) {
    return nca_loader->ReadRomFS(out_file);
}

u64 AppLoader_XCI::ReadRomFSIVFCOffset() const {
    return nca_loader->ReadRomFSIVFCOffset();
}

ResultStatus AppLoader_XCI::ReadUpdateRaw(FileSys::VirtualFile& out_file) {
    u64 program_id{};
    nca_loader->ReadProgramId(program_id);
    if (program_id == 0) {
        return ResultStatus::ErrorXCIMissingProgramNCA;
    }

    const auto read = xci->GetSecurePartitionNSP()->GetNCAFile(
        FileSys::GetUpdateTitleID(program_id), FileSys::ContentRecordType::Program);
    if (read == nullptr) {
        return ResultStatus::ErrorNoPackedUpdate;
    }

    const auto nca_test = std::make_shared<FileSys::NCA>(read);
    if (nca_test->GetStatus() != ResultStatus::ErrorMissingBKTRBaseRomFS) {
        return nca_test->GetStatus();
    }

    out_file = read;
    return ResultStatus::Success;
}

ResultStatus AppLoader_XCI::ReadProgramId(u64& out_program_id) {
    return nca_loader->ReadProgramId(out_program_id);
}

ResultStatus AppLoader_XCI::ReadProgramIds(std::vector<u64>& out_program_ids) {
    out_program_ids = xci->GetProgramTitleIDs();
    return ResultStatus::Success;
}

ResultStatus AppLoader_XCI::ReadIcon(std::vector<u8>& buffer) {
    if (icon_file == nullptr) {
        return ResultStatus::ErrorNoControl;
    }

    buffer = icon_file->ReadAllBytes();
    return ResultStatus::Success;
}

ResultStatus AppLoader_XCI::ReadTitle(std::string& title) {
    if (nacp_file == nullptr) {
        return ResultStatus::ErrorNoControl;
    }

    title = nacp_file->GetApplicationName();
    return ResultStatus::Success;
}

ResultStatus AppLoader_XCI::ReadControlData(FileSys::NACP& control) {
    if (nacp_file == nullptr) {
        return ResultStatus::ErrorNoControl;
    }

    control = *nacp_file;
    return ResultStatus::Success;
}

ResultStatus AppLoader_XCI::ReadManualRomFS(FileSys::VirtualFile& out_file) {
    const auto nca =
        xci->GetSecurePartitionNSP()->GetNCA(xci->GetSecurePartitionNSP()->GetProgramTitleID(),
                                             FileSys::ContentRecordType::HtmlDocument);
    if (xci->GetStatus() != ResultStatus::Success || nca == nullptr) {
        return ResultStatus::ErrorXCIMissingPartition;
    }

    out_file = nca->GetRomFS();
    return out_file == nullptr ? ResultStatus::ErrorNoRomFS : ResultStatus::Success;
}

ResultStatus AppLoader_XCI::ReadBanner(std::vector<u8>& buffer) {
    return nca_loader->ReadBanner(buffer);
}

ResultStatus AppLoader_XCI::ReadLogo(std::vector<u8>& buffer) {
    return nca_loader->ReadLogo(buffer);
}

ResultStatus AppLoader_XCI::ReadNSOModules(Modules& modules) {
    return nca_loader->ReadNSOModules(modules);
}

} // namespace Loader
