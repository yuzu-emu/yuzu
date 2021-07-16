// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <string>

#include <fmt/ostream.h>

#include "common/logging/log.h"
#include "core/crypto/key_manager.h"
#include "core/file_sys/card_image.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/partition_filesystem.h"
#include "core/file_sys/submission_package.h"
#include "core/file_sys/vfs_concat.h"
#include "core/file_sys/vfs_offset.h"
#include "core/file_sys/vfs_vector.h"
#include "core/loader/loader.h"

namespace FileSys {

constexpr u64 GAMECARD_CERTIFICATE_OFFSET = 0x7000;
constexpr std::array partition_names{
    "update",
    "normal",
    "secure",
    "logo",
};

XCI::XCI(VirtualFile file_, u64 program_id, size_t program_index)
    : file(std::move(file_)), program_nca_status{Loader::ResultStatus::ErrorXCIMissingProgramNCA},
      partitions(partition_names.size()),
      partitions_raw(partition_names.size()), keys{Core::Crypto::KeyManager::Instance()} {
    if (file->ReadObject(&header) != sizeof(GamecardHeader)) {
        status = Loader::ResultStatus::ErrorBadXCIHeader;
        return;
    }

    if (header.magic != Common::MakeMagic('H', 'E', 'A', 'D')) {
        status = Loader::ResultStatus::ErrorBadXCIHeader;
        return;
    }

    PartitionFilesystem main_hfs(std::make_shared<OffsetVfsFile>(
        file, file->GetSize() - header.hfs_offset, header.hfs_offset));

    update_normal_partition_end = main_hfs.GetFileOffsets()["secure"];

    if (main_hfs.GetStatus() != Loader::ResultStatus::Success) {
        status = main_hfs.GetStatus();
        return;
    }

    for (XCIPartition partition :
         {XCIPartition::Update, XCIPartition::Normal, XCIPartition::Secure, XCIPartition::Logo}) {
        const auto partition_idx = static_cast<std::size_t>(partition);
        auto raw = main_hfs.GetFile(partition_names[partition_idx]);

        partitions_raw[static_cast<std::size_t>(partition)] = std::move(raw);
    }

    secure_partition = std::make_shared<NSP>(
        main_hfs.GetFile(partition_names[static_cast<std::size_t>(XCIPartition::Secure)]),
        program_id, program_index);

    ncas = secure_partition->GetNCAsCollapsed();
    program =
        secure_partition->GetNCA(secure_partition->GetProgramTitleID(), ContentRecordType::Program);
    program_nca_status = secure_partition->GetProgramStatus();
    if (program_nca_status == Loader::ResultStatus::ErrorNSPMissingProgramNCA) {
        program_nca_status = Loader::ResultStatus::ErrorXCIMissingProgramNCA;
    }

    auto result = AddNCAFromPartition(XCIPartition::Normal);
    if (result != Loader::ResultStatus::Success) {
        status = result;
        return;
    }

    if (GetFormatVersion() >= 0x2) {
        result = AddNCAFromPartition(XCIPartition::Logo);
        if (result != Loader::ResultStatus::Success) {
            status = result;
            return;
        }
    }

    status = Loader::ResultStatus::Success;
}

XCI::~XCI() = default;

Loader::ResultStatus XCI::GetStatus() const {
    return status;
}

Loader::ResultStatus XCI::GetProgramNCAStatus() const {
    return program_nca_status;
}

VirtualDir XCI::GetPartition(XCIPartition partition) {
    const auto id = static_cast<std::size_t>(partition);
    if (partitions[id] == nullptr && partitions_raw[id] != nullptr) {
        partitions[id] = std::make_shared<PartitionFilesystem>(partitions_raw[id]);
    }

    return partitions[static_cast<std::size_t>(partition)];
}

std::vector<VirtualDir> XCI::GetPartitions() {
    std::vector<VirtualDir> out;
    for (const auto& id :
         {XCIPartition::Update, XCIPartition::Normal, XCIPartition::Secure, XCIPartition::Logo}) {
        const auto part = GetPartition(id);
        if (part != nullptr) {
            out.push_back(part);
        }
    }
    return out;
}

std::shared_ptr<NSP> XCI::GetSecurePartitionNSP() const {
    return secure_partition;
}

VirtualDir XCI::GetSecurePartition() {
    return GetPartition(XCIPartition::Secure);
}

VirtualDir XCI::GetNormalPartition() {
    return GetPartition(XCIPartition::Normal);
}

VirtualDir XCI::GetUpdatePartition() {
    return GetPartition(XCIPartition::Update);
}

VirtualDir XCI::GetLogoPartition() {
    return GetPartition(XCIPartition::Logo);
}

VirtualFile XCI::GetPartitionRaw(XCIPartition partition) const {
    return partitions_raw[static_cast<std::size_t>(partition)];
}

VirtualFile XCI::GetSecurePartitionRaw() const {
    return GetPartitionRaw(XCIPartition::Secure);
}

VirtualFile XCI::GetStoragePartition0() const {
    return std::make_shared<OffsetVfsFile>(file, update_normal_partition_end, 0, "partition0");
}

VirtualFile XCI::GetStoragePartition1() const {
    return std::make_shared<OffsetVfsFile>(file, file->GetSize() - update_normal_partition_end,
                                           update_normal_partition_end, "partition1");
}

VirtualFile XCI::GetNormalPartitionRaw() const {
    return GetPartitionRaw(XCIPartition::Normal);
}

VirtualFile XCI::GetUpdatePartitionRaw() const {
    return GetPartitionRaw(XCIPartition::Update);
}

VirtualFile XCI::GetLogoPartitionRaw() const {
    return GetPartitionRaw(XCIPartition::Logo);
}

u64 XCI::GetProgramTitleID() const {
    return secure_partition->GetProgramTitleID();
}

std::vector<u64> XCI::GetProgramTitleIDs() const {
    return secure_partition->GetProgramTitleIDs();
}

u32 XCI::GetSystemUpdateVersion() {
    const auto update = GetPartition(XCIPartition::Update);
    if (update == nullptr) {
        return 0;
    }

    for (const auto& update_file : update->GetFiles()) {
        NCA nca{update_file, nullptr, 0};

        if (nca.GetStatus() != Loader::ResultStatus::Success) {
            continue;
        }

        if (nca.GetType() == NCAContentType::Meta && nca.GetTitleId() == 0x0100000000000816) {
            const auto dir = nca.GetSubdirectories()[0];
            const auto cnmt = dir->GetFile("SystemUpdate_0100000000000816.cnmt");
            if (cnmt == nullptr) {
                continue;
            }

            CNMT cnmt_data{cnmt};

            const auto metas = cnmt_data.GetMetaRecords();
            if (metas.empty()) {
                continue;
            }

            return metas[0].title_version;
        }
    }

    return 0;
}

u64 XCI::GetSystemUpdateTitleID() const {
    return 0x0100000000000816;
}

bool XCI::HasProgramNCA() const {
    return program != nullptr;
}

VirtualFile XCI::GetProgramNCAFile() const {
    if (!HasProgramNCA()) {
        return nullptr;
    }

    return program->GetBaseFile();
}

const std::vector<std::shared_ptr<NCA>>& XCI::GetNCAs() const {
    return ncas;
}

std::shared_ptr<NCA> XCI::GetNCAByType(NCAContentType type) const {
    const auto program_id = secure_partition->GetProgramTitleID();
    const auto iter = std::find_if(
        ncas.begin(), ncas.end(), [this, type, program_id](const std::shared_ptr<NCA>& nca) {
            return nca->GetType() == type && nca->GetTitleId() == program_id;
        });
    return iter == ncas.end() ? nullptr : *iter;
}

VirtualFile XCI::GetNCAFileByType(NCAContentType type) const {
    auto nca = GetNCAByType(type);
    if (nca != nullptr) {
        return nca->GetBaseFile();
    }
    return nullptr;
}

std::vector<VirtualFile> XCI::GetFiles() const {
    return {};
}

std::vector<VirtualDir> XCI::GetSubdirectories() const {
    return {};
}

std::string XCI::GetName() const {
    return file->GetName();
}

VirtualDir XCI::GetParentDirectory() const {
    return file->GetContainingDirectory();
}

VirtualDir XCI::ConcatenatedPseudoDirectory() {
    const auto out = std::make_shared<VectorVfsDirectory>();
    for (const auto& part_id : {XCIPartition::Normal, XCIPartition::Logo, XCIPartition::Secure}) {
        const auto& part = GetPartition(part_id);
        if (part == nullptr)
            continue;

        for (const auto& part_file : part->GetFiles())
            out->AddFile(part_file);
    }

    return out;
}

std::array<u8, 0x200> XCI::GetCertificate() const {
    std::array<u8, 0x200> out;
    file->Read(out.data(), out.size(), GAMECARD_CERTIFICATE_OFFSET);
    return out;
}

Loader::ResultStatus XCI::AddNCAFromPartition(XCIPartition part) {
    const auto partition_index = static_cast<std::size_t>(part);
    const auto partition = GetPartition(part);

    if (partition == nullptr) {
        return Loader::ResultStatus::ErrorXCIMissingPartition;
    }

    for (const VirtualFile& partition_file : partition->GetFiles()) {
        if (partition_file->GetExtension() != "nca") {
            continue;
        }

        auto nca = std::make_shared<NCA>(partition_file, nullptr, 0);
        if (nca->IsUpdate()) {
            continue;
        }
        if (nca->GetType() == NCAContentType::Program) {
            program_nca_status = nca->GetStatus();
        }
        if (nca->GetStatus() == Loader::ResultStatus::Success) {
            ncas.push_back(std::move(nca));
        } else {
            const u16 error_id = static_cast<u16>(nca->GetStatus());
            LOG_CRITICAL(Loader, "Could not load NCA {}/{}, failed with error code {:04X} ({})",
                         partition_names[partition_index], nca->GetName(), error_id,
                         nca->GetStatus());
        }
    }

    return Loader::ResultStatus::Success;
}

u8 XCI::GetFormatVersion() {
    return GetLogoPartition() == nullptr ? 0x1 : 0x2;
}
} // namespace FileSys
