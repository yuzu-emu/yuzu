// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "common/common_types.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/vfs.h"

namespace Core {
class System;
}

namespace FileSys {
class NACP;
} // namespace FileSys

namespace Kernel {
struct AddressMapping;
class KProcess;
} // namespace Kernel

namespace Loader {

/// File types supported by CTR
enum class FileType {
    Error,
    Unknown,
    ELF,
    NSO,
    NRO,
    NCA,
    NSP,
    XCI,
    NAX,
    KIP,
    DeconstructedRomDirectory,
};

/**
 * Identifies the type of a bootable file based on the magic value in its header.
 * @param file open file
 * @return FileType of file
 */
FileType IdentifyFile(FileSys::VirtualFile file);

/**
 * Guess the type of a bootable file from its name
 * @param name String name of bootable file
 * @return FileType of file. Note: this will return FileType::Unknown if it is unable to determine
 * a filetype, and will never return FileType::Error.
 */
FileType GuessFromFilename(const std::string& name);

/**
 * Convert a FileType into a string which can be displayed to the user.
 */
std::string GetFileTypeString(FileType type);

/// Return type for functions in Loader namespace
enum class ResultStatus : u16 {
    Success,
    ErrorAlreadyLoaded,
    ErrorNotImplemented,
    ErrorNotInitialized,
    ErrorBadNPDMHeader,
    ErrorBadACIDHeader,
    ErrorBadACIHeader,
    ErrorBadFileAccessControl,
    ErrorBadFileAccessHeader,
    ErrorBadKernelCapabilityDescriptors,
    ErrorBadPFSHeader,
    ErrorIncorrectPFSFileSize,
    ErrorBadNCAHeader,
    ErrorMissingProductionKeyFile,
    ErrorMissingHeaderKey,
    ErrorIncorrectHeaderKey,
    ErrorNCA2,
    ErrorNCA0,
    ErrorMissingTitlekey,
    ErrorMissingTitlekek,
    ErrorInvalidRightsID,
    ErrorMissingKeyAreaKey,
    ErrorIncorrectKeyAreaKey,
    ErrorIncorrectTitlekeyOrTitlekek,
    ErrorXCIMissingProgramNCA,
    ErrorNCANotProgram,
    ErrorNoExeFS,
    ErrorBadXCIHeader,
    ErrorXCIMissingPartition,
    ErrorNullFile,
    ErrorMissingNPDM,
    Error32BitISA,
    ErrorUnableToParseKernelMetadata,
    ErrorNoRomFS,
    ErrorIncorrectELFFileSize,
    ErrorLoadingNRO,
    ErrorLoadingNSO,
    ErrorNoIcon,
    ErrorNoControl,
    ErrorBadNAXHeader,
    ErrorIncorrectNAXFileSize,
    ErrorNAXKeyHMACFailed,
    ErrorNAXValidationHMACFailed,
    ErrorNAXKeyDerivationFailed,
    ErrorNAXInconvertibleToNCA,
    ErrorBadNAXFilePath,
    ErrorMissingSDSeed,
    ErrorMissingSDKEKSource,
    ErrorMissingAESKEKGenerationSource,
    ErrorMissingAESKeyGenerationSource,
    ErrorMissingSDSaveKeySource,
    ErrorMissingSDNCAKeySource,
    ErrorNSPMissingProgramNCA,
    ErrorBadBKTRHeader,
    ErrorBKTRSubsectionNotAfterRelocation,
    ErrorBKTRSubsectionNotAtEnd,
    ErrorBadRelocationBlock,
    ErrorBadSubsectionBlock,
    ErrorBadRelocationBuckets,
    ErrorBadSubsectionBuckets,
    ErrorMissingBKTRBaseRomFS,
    ErrorNoPackedUpdate,
    ErrorBadKIPHeader,
    ErrorBLZDecompressionFailed,
    ErrorBadINIHeader,
    ErrorINITooManyKIPs,
};

std::string GetResultStatusString(ResultStatus status);
std::ostream& operator<<(std::ostream& os, ResultStatus status);

/// Interface for loading an application
class AppLoader : NonCopyable {
public:
    struct LoadParameters {
        s32 main_thread_priority;
        u64 main_thread_stack_size;
    };
    using LoadResult = std::pair<ResultStatus, std::optional<LoadParameters>>;

    explicit AppLoader(FileSys::VirtualFile file_);
    virtual ~AppLoader();

    /**
     * Returns the type of this file
     *
     * @return FileType corresponding to the loaded file
     */
    virtual FileType GetFileType() const = 0;

    /**
     * Load the application and return the created Process instance
     *
     * @param process The newly created process.
     * @param system  The system that this process is being loaded under.
     *
     * @return The status result of the operation.
     */
    virtual LoadResult Load(Kernel::KProcess& process, Core::System& system) = 0;

    /**
     * Get the code (typically .code section) of the application
     *
     * @param[out] buffer Reference to buffer to store data
     *
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadCode(std::vector<u8>& buffer) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the icon (typically icon section) of the application
     *
     * @param[out] buffer Reference to buffer to store data
     *
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadIcon(std::vector<u8>& buffer) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the banner (typically banner section) of the application
     * In the context of NX, this is the animation that displays in the bottom right of the screen
     * when a game boots. Stored in GIF format.
     *
     * @param[out] buffer Reference to buffer to store data
     *
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadBanner(std::vector<u8>& buffer) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the logo (typically logo section) of the application
     * In the context of NX, this is the static image that displays in the top left of the screen
     * when a game boots. Stored in JPEG format.
     *
     * @param[out] buffer Reference to buffer to store data
     *
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadLogo(std::vector<u8>& buffer) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the program id of the application
     *
     * @param[out] out_program_id Reference to store program id into
     *
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadProgramId(u64& out_program_id) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the program ids of the application
     *
     * @param[out] out_program_ids Reference to store program ids into
     *
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadProgramIds(std::vector<u64>& out_program_ids) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the RomFS of the application
     * Since the RomFS can be huge, we return a file reference instead of copying to a buffer
     *
     * @param[out] out_file The directory containing the RomFS
     *
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadRomFS(FileSys::VirtualFile& out_file) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the raw update of the application, should it come packed with one
     *
     * @param[out] out_file The raw update NCA file (Program-type)
     *
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadUpdateRaw(FileSys::VirtualFile& out_file) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get whether or not updates can be applied to the RomFS.
     * By default, this is true, however for formats where it cannot be guaranteed that the RomFS is
     * the base game it should be set to false.
     *
     * @return bool indicating whether or not the RomFS is updatable.
     */
    virtual bool IsRomFSUpdatable() const {
        return true;
    }

    /**
     * Gets the difference between the start of the IVFC header and the start of level 6 (RomFS)
     * data. Needed for BKTR patching.
     *
     * @return IVFC offset for RomFS.
     */
    virtual u64 ReadRomFSIVFCOffset() const {
        return 0;
    }

    /**
     * Get the title of the application
     *
     * @param[out] title Reference to store the application title into
     *
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadTitle(std::string& title) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the control data (CNMT) of the application
     *
     * @param[out] control Reference to store the application control data into
     *
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadControlData(FileSys::NACP& control) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the RomFS of the manual of the application
     *
     * @param[out] out_file The raw manual RomFS of the game
     *
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadManualRomFS(FileSys::VirtualFile& out_file) {
        return ResultStatus::ErrorNotImplemented;
    }

    using Modules = std::map<VAddr, std::string>;

    virtual ResultStatus ReadNSOModules(Modules& modules) {
        return ResultStatus::ErrorNotImplemented;
    }

protected:
    FileSys::VirtualFile file;
    bool is_loaded = false;
};

/**
 * Identifies a bootable file and return a suitable loader
 *
 * @param system The system context.
 * @param file   The bootable file.
 * @param program_index Specifies the index within the container of the program to launch.
 *
 * @return the best loader for this file.
 */
std::unique_ptr<AppLoader> GetLoader(Core::System& system, FileSys::VirtualFile file,
                                     u64 program_id = 0, std::size_t program_index = 0);

} // namespace Loader
