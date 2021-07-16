// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "common/common_types.h"
#include "core/file_sys/vfs.h"
#include "core/loader/loader.h"

namespace FileSys {
class ContentProvider;
class NACP;
class XCI;
} // namespace FileSys

namespace Service::FileSystem {
class FileSystemController;
}

namespace Loader {

class AppLoader_NCA;

/// Loads an XCI file
class AppLoader_XCI final : public AppLoader {
public:
    explicit AppLoader_XCI(FileSys::VirtualFile file_,
                           const Service::FileSystem::FileSystemController& fsc,
                           const FileSys::ContentProvider& content_provider, u64 program_id,
                           std::size_t program_index);
    ~AppLoader_XCI() override;

    /**
     * Identifies whether or not the given file is an XCI file.
     *
     * @param xci_file The file to identify.
     *
     * @return FileType::XCI, or FileType::Error if the file is not an XCI file.
     */
    static FileType IdentifyType(const FileSys::VirtualFile& xci_file);

    FileType GetFileType() const override {
        return IdentifyType(file);
    }

    LoadResult Load(Kernel::KProcess& process, Core::System& system) override;

    ResultStatus ReadRomFS(FileSys::VirtualFile& out_file) override;
    u64 ReadRomFSIVFCOffset() const override;
    ResultStatus ReadUpdateRaw(FileSys::VirtualFile& out_file) override;
    ResultStatus ReadProgramId(u64& out_program_id) override;
    ResultStatus ReadProgramIds(std::vector<u64>& out_program_ids) override;
    ResultStatus ReadIcon(std::vector<u8>& buffer) override;
    ResultStatus ReadTitle(std::string& title) override;
    ResultStatus ReadControlData(FileSys::NACP& control) override;
    ResultStatus ReadManualRomFS(FileSys::VirtualFile& out_file) override;

    ResultStatus ReadBanner(std::vector<u8>& buffer) override;
    ResultStatus ReadLogo(std::vector<u8>& buffer) override;

    ResultStatus ReadNSOModules(Modules& modules) override;

private:
    std::unique_ptr<FileSys::XCI> xci;
    std::unique_ptr<AppLoader_NCA> nca_loader;

    FileSys::VirtualFile icon_file;
    std::unique_ptr<FileSys::NACP> nacp_file;
};

} // namespace Loader
