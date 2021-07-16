// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <vector>

#include "common/common_types.h"
#include "core/core.h"
#include "core/file_sys/card_image.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/submission_package.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/loader/deconstructed_rom_directory.h"
#include "core/loader/nca.h"
#include "core/loader/nsp.h"

namespace Loader {

AppLoader_NSP::AppLoader_NSP(FileSys::VirtualFile file_,
                             const Service::FileSystem::FileSystemController& fsc,
                             const FileSys::ContentProvider& content_provider, u64 program_id,
                             std::size_t program_index)
    : AppLoader(file_), nsp(std::make_unique<FileSys::NSP>(file_, program_id, program_index)) {

    if (nsp->GetStatus() != ResultStatus::Success) {
        return;
    }

    if (nsp->IsExtractedType()) {
        secondary_loader = std::make_unique<AppLoader_DeconstructedRomDirectory>(nsp->GetExeFS());
    } else {
        const auto control_nca =
            nsp->GetNCA(nsp->GetProgramTitleID(), FileSys::ContentRecordType::Control);
        if (control_nca == nullptr || control_nca->GetStatus() != ResultStatus::Success) {
            return;
        }

        std::tie(nacp_file, icon_file) = [this, &content_provider, &control_nca, &fsc] {
            const FileSys::PatchManager pm{nsp->GetProgramTitleID(), fsc, content_provider};
            return pm.ParseControlNCA(*control_nca);
        }();

        secondary_loader = std::make_unique<AppLoader_NCA>(
            nsp->GetNCAFile(nsp->GetProgramTitleID(), FileSys::ContentRecordType::Program));
    }
}

AppLoader_NSP::~AppLoader_NSP() = default;

FileType AppLoader_NSP::IdentifyType(const FileSys::VirtualFile& nsp_file) {
    const FileSys::NSP nsp(nsp_file);

    if (nsp.GetStatus() == ResultStatus::Success) {
        // Extracted Type case
        if (nsp.IsExtractedType() && nsp.GetExeFS() != nullptr &&
            FileSys::IsDirectoryExeFS(nsp.GetExeFS())) {
            return FileType::NSP;
        }

        // Non-Extracted Type case
        const auto program_id = nsp.GetProgramTitleID();
        if (!nsp.IsExtractedType() &&
            nsp.GetNCA(program_id, FileSys::ContentRecordType::Program) != nullptr &&
            AppLoader_NCA::IdentifyType(
                nsp.GetNCAFile(program_id, FileSys::ContentRecordType::Program)) == FileType::NCA) {
            return FileType::NSP;
        }
    }

    return FileType::Error;
}

AppLoader_NSP::LoadResult AppLoader_NSP::Load(Kernel::KProcess& process, Core::System& system) {
    if (is_loaded) {
        return {ResultStatus::ErrorAlreadyLoaded, {}};
    }

    const auto title_id = nsp->GetProgramTitleID();

    if (!nsp->IsExtractedType() && title_id == 0) {
        return {ResultStatus::ErrorNSPMissingProgramNCA, {}};
    }

    const auto nsp_status = nsp->GetStatus();
    if (nsp_status != ResultStatus::Success) {
        return {nsp_status, {}};
    }

    const auto nsp_program_status = nsp->GetProgramStatus();
    if (nsp_program_status != ResultStatus::Success) {
        return {nsp_program_status, {}};
    }

    if (!nsp->IsExtractedType() &&
        nsp->GetNCA(title_id, FileSys::ContentRecordType::Program) == nullptr) {
        if (!Core::Crypto::KeyManager::KeyFileExists(false)) {
            return {ResultStatus::ErrorMissingProductionKeyFile, {}};
        }

        return {ResultStatus::ErrorNSPMissingProgramNCA, {}};
    }

    const auto result = secondary_loader->Load(process, system);
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

ResultStatus AppLoader_NSP::ReadRomFS(FileSys::VirtualFile& out_file) {
    return secondary_loader->ReadRomFS(out_file);
}

u64 AppLoader_NSP::ReadRomFSIVFCOffset() const {
    return secondary_loader->ReadRomFSIVFCOffset();
}

ResultStatus AppLoader_NSP::ReadUpdateRaw(FileSys::VirtualFile& out_file) {
    if (nsp->IsExtractedType()) {
        return ResultStatus::ErrorNoPackedUpdate;
    }

    const auto read = nsp->GetNCAFile(FileSys::GetUpdateTitleID(nsp->GetProgramTitleID()),
                                      FileSys::ContentRecordType::Program);

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

ResultStatus AppLoader_NSP::ReadProgramId(u64& out_program_id) {
    out_program_id = nsp->GetProgramTitleID();
    if (out_program_id == 0) {
        return ResultStatus::ErrorNotInitialized;
    }
    return ResultStatus::Success;
}

ResultStatus AppLoader_NSP::ReadProgramIds(std::vector<u64>& out_program_ids) {
    out_program_ids = nsp->GetProgramTitleIDs();
    return ResultStatus::Success;
}

ResultStatus AppLoader_NSP::ReadIcon(std::vector<u8>& buffer) {
    if (icon_file == nullptr) {
        return ResultStatus::ErrorNoControl;
    }

    buffer = icon_file->ReadAllBytes();
    return ResultStatus::Success;
}

ResultStatus AppLoader_NSP::ReadTitle(std::string& title) {
    if (nacp_file == nullptr) {
        return ResultStatus::ErrorNoControl;
    }

    title = nacp_file->GetApplicationName();
    return ResultStatus::Success;
}

ResultStatus AppLoader_NSP::ReadControlData(FileSys::NACP& nacp) {
    if (nacp_file == nullptr) {
        return ResultStatus::ErrorNoControl;
    }

    nacp = *nacp_file;
    return ResultStatus::Success;
}

ResultStatus AppLoader_NSP::ReadManualRomFS(FileSys::VirtualFile& out_file) {
    const auto nca =
        nsp->GetNCA(nsp->GetProgramTitleID(), FileSys::ContentRecordType::HtmlDocument);
    if (nsp->GetStatus() != ResultStatus::Success || nca == nullptr) {
        return ResultStatus::ErrorNoRomFS;
    }

    out_file = nca->GetRomFS();
    return out_file == nullptr ? ResultStatus::ErrorNoRomFS : ResultStatus::Success;
}

ResultStatus AppLoader_NSP::ReadBanner(std::vector<u8>& buffer) {
    return secondary_loader->ReadBanner(buffer);
}

ResultStatus AppLoader_NSP::ReadLogo(std::vector<u8>& buffer) {
    return secondary_loader->ReadLogo(buffer);
}

ResultStatus AppLoader_NSP::ReadNSOModules(Modules& modules) {
    return secondary_loader->ReadNSOModules(modules);
}

} // namespace Loader
