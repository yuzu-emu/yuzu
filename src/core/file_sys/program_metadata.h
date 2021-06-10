// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <vector>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/swap.h"
#include "core/file_sys/vfs_types.h"

namespace Loader {
enum class ResultStatus : u16;
}

namespace FileSys {

enum class ProgramAddressSpaceType : u8 {
    Is32Bit = 0,
    Is36Bit = 1,
    Is32BitNoMap = 2,
    Is39Bit = 3,
};

enum class ProgramFilePermission : u64 {
    MountContent = 1ULL << 0,
    SaveDataBackup = 1ULL << 5,
    SdCard = 1ULL << 21,
    Calibration = 1ULL << 34,
    Bit62 = 1ULL << 62,
    Everything = 1ULL << 63,
};

/**
 * Helper which implements an interface to parse Program Description Metadata (NPDM)
 * Data can either be loaded from a file path or with data and an offset into it.
 */
class ProgramMetadata {
public:
    using KernelCapabilityDescriptors = std::vector<u32>;

    ProgramMetadata();
    ~ProgramMetadata();

    ProgramMetadata(const ProgramMetadata&) = default;
    ProgramMetadata& operator=(const ProgramMetadata&) = default;

    ProgramMetadata(ProgramMetadata&&) = default;
    ProgramMetadata& operator=(ProgramMetadata&&) = default;

    /// Gets a default ProgramMetadata configuration, should only be used for homebrew formats where
    /// we do not have an NPDM file
    static ProgramMetadata GetDefault();

    Loader::ResultStatus Load(VirtualFile file);

    /// Load from parameters instead of NPDM file, used for KIP
    void LoadManual(bool is_64_bit, ProgramAddressSpaceType address_space, s32 main_thread_prio,
                    u32 main_thread_core, u32 main_thread_stack_size, u64 title_id,
                    u64 filesystem_permissions, u32 system_resource_size,
                    KernelCapabilityDescriptors capabilities);

    bool Is64BitProgram() const;
    ProgramAddressSpaceType GetAddressSpaceType() const;
    u8 GetMainThreadPriority() const;
    u8 GetMainThreadCore() const;
    u32 GetMainThreadStackSize() const;
    u64 GetTitleID() const;
    u64 GetFilesystemPermissions() const;
    u32 GetSystemResourceSize() const;
    const KernelCapabilityDescriptors& GetKernelCapabilities() const;

    void Print() const;

private:
    struct Header {
        std::array<char, 4> magic;
        std::array<u8, 8> reserved;
        union {
            u8 flags;

            BitField<0, 1, u8> has_64_bit_instructions;
            BitField<1, 3, ProgramAddressSpaceType> address_space_type;
            BitField<4, 4, u8> reserved_2;
        };
        u8 reserved_3;
        u8 main_thread_priority;
        u8 main_thread_cpu;
        std::array<u8, 4> reserved_4;
        u32_le system_resource_size;
        u32_le process_category;
        u32_le main_stack_size;
        std::array<u8, 0x10> application_name;
        std::array<u8, 0x40> reserved_5;
        u32_le aci_offset;
        u32_le aci_size;
        u32_le acid_offset;
        u32_le acid_size;
    };

    static_assert(sizeof(Header) == 0x80, "NPDM header structure size is wrong");

    struct AcidHeader {
        std::array<u8, 0x100> signature;
        std::array<u8, 0x100> nca_modulus;
        std::array<char, 4> magic;
        u32_le nca_size;
        std::array<u8, 0x4> reserved;
        union {
            u32 flags;

            BitField<0, 1, u32> is_retail;
            BitField<1, 31, u32> flags_unk;
        };
        u64_le title_id_min;
        u64_le title_id_max;
        u32_le fac_offset;
        u32_le fac_size;
        u32_le sac_offset;
        u32_le sac_size;
        u32_le kac_offset;
        u32_le kac_size;
        INSERT_PADDING_BYTES(0x8);
    };

    static_assert(sizeof(AcidHeader) == 0x240, "ACID header structure size is wrong");

    struct AciHeader {
        std::array<char, 4> magic;
        std::array<u8, 0xC> reserved;
        u64_le title_id;
        INSERT_PADDING_BYTES(0x8);
        u32_le fah_offset;
        u32_le fah_size;
        u32_le sac_offset;
        u32_le sac_size;
        u32_le kac_offset;
        u32_le kac_size;
        INSERT_PADDING_BYTES(0x8);
    };

    static_assert(sizeof(AciHeader) == 0x40, "ACI0 header structure size is wrong");

    // FileAccessControl and FileAccessHeader need loaded per-component: this layout does not
    // reflect the real layout to avoid reference binding to misaligned addresses
    struct FileAccessControl {
        u8 version;
        // 3 padding bytes
        u64_le permissions;
        std::array<u8, 0x20> unknown;
    };

    struct FileAccessHeader {
        u8 version;
        // 3 padding bytes
        u64_le permissions;
        u32_le unk_offset;
        u32_le unk_size;
        u32_le unk_offset_2;
        u32_le unk_size_2;
    };

    Header npdm_header;
    AciHeader aci_header;
    AcidHeader acid_header;

    FileAccessControl acid_file_access;
    FileAccessHeader aci_file_access;

    KernelCapabilityDescriptors aci_kernel_capabilities;
};

} // namespace FileSys
