// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/alignment.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/kernel/k_page_table.h"
#include "core/hle/kernel/k_process.h"
#include "core/memory.h"
#include "video_core/gpu.h"
#include "video_core/memory_manager.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_base.h"

namespace Tegra {

MemoryManager::MemoryManager(Core::System& system_)
    : system{system_}, page_table(page_table_size) {}

MemoryManager::~MemoryManager() = default;

void MemoryManager::BindRasterizer(VideoCore::RasterizerInterface* rasterizer_) {
    rasterizer = rasterizer_;
}

GPUVAddr MemoryManager::UpdateRange(GPUVAddr gpu_addr, PageEntry page_entry, std::size_t size) {
    u64 remaining_size{size};
    for (u64 offset{}; offset < size; offset += page_size) {
        if (remaining_size < page_size) {
            SetPageEntry(gpu_addr + offset, page_entry + offset, remaining_size);
        } else {
            SetPageEntry(gpu_addr + offset, page_entry + offset);
        }
        remaining_size -= page_size;
    }
    return gpu_addr;
}

GPUVAddr MemoryManager::Map(VAddr cpu_addr, GPUVAddr gpu_addr, std::size_t size) {
    const auto it = std::ranges::lower_bound(map_ranges, gpu_addr, {}, &MapRange::first);
    if (it != map_ranges.end() && it->first == gpu_addr) {
        it->second = size;
    } else {
        map_ranges.insert(it, MapRange{gpu_addr, size});
    }
    return UpdateRange(gpu_addr, cpu_addr, size);
}

GPUVAddr MemoryManager::MapAllocate(VAddr cpu_addr, std::size_t size, std::size_t align) {
    return Map(cpu_addr, *FindFreeRange(size, align), size);
}

GPUVAddr MemoryManager::MapAllocate32(VAddr cpu_addr, std::size_t size) {
    const std::optional<GPUVAddr> gpu_addr = FindFreeRange(size, 1, true);
    ASSERT(gpu_addr);
    return Map(cpu_addr, *gpu_addr, size);
}

void MemoryManager::Unmap(GPUVAddr gpu_addr, std::size_t size) {
    if (size == 0) {
        return;
    }
    const auto it = std::ranges::lower_bound(map_ranges, gpu_addr, {}, &MapRange::first);
    if (it != map_ranges.end()) {
        ASSERT(it->first == gpu_addr);
        map_ranges.erase(it);
    } else {
        UNREACHABLE_MSG("Unmapping non-existent GPU address=0x{:x}", gpu_addr);
    }
    const auto submapped_ranges = GetSubmappedRange(gpu_addr, size);

    for (const auto& map : submapped_ranges) {
        // Flush and invalidate through the GPU interface, to be asynchronous if possible.
        const std::optional<VAddr> cpu_addr = GpuToCpuAddress(map.first);
        ASSERT(cpu_addr);

        rasterizer->UnmapMemory(*cpu_addr, map.second);
    }

    UpdateRange(gpu_addr, PageEntry::State::Unmapped, size);
}

std::optional<GPUVAddr> MemoryManager::AllocateFixed(GPUVAddr gpu_addr, std::size_t size) {
    for (u64 offset{}; offset < size; offset += page_size) {
        if (!GetPageEntry(gpu_addr + offset).IsUnmapped()) {
            return std::nullopt;
        }
    }

    return UpdateRange(gpu_addr, PageEntry::State::Allocated, size);
}

GPUVAddr MemoryManager::Allocate(std::size_t size, std::size_t align) {
    return *AllocateFixed(*FindFreeRange(size, align), size);
}

void MemoryManager::TryLockPage(PageEntry page_entry, std::size_t size) {
    if (!page_entry.IsValid()) {
        return;
    }

    ASSERT(system.CurrentProcess()
               ->PageTable()
               .LockForDeviceAddressSpace(page_entry.ToAddress(), size)
               .IsSuccess());
}

void MemoryManager::TryUnlockPage(PageEntry page_entry, std::size_t size) {
    if (!page_entry.IsValid()) {
        return;
    }

    ASSERT(system.CurrentProcess()
               ->PageTable()
               .UnlockForDeviceAddressSpace(page_entry.ToAddress(), size)
               .IsSuccess());
}

PageEntry MemoryManager::GetPageEntry(GPUVAddr gpu_addr) const {
    return page_table[PageEntryIndex(gpu_addr)];
}

void MemoryManager::SetPageEntry(GPUVAddr gpu_addr, PageEntry page_entry, std::size_t size) {
    // TODO(bunnei): We should lock/unlock device regions. This currently causes issues due to
    // improper tracking, but should be fixed in the future.

    //// Unlock the old page
    // TryUnlockPage(page_table[PageEntryIndex(gpu_addr)], size);

    //// Lock the new page
    // TryLockPage(page_entry, size);
    auto& current_page = page_table[PageEntryIndex(gpu_addr)];

    if ((!current_page.IsValid() && page_entry.IsValid()) ||
        current_page.ToAddress() != page_entry.ToAddress()) {
        rasterizer->ModifyGPUMemory(gpu_addr, size);
    }

    current_page = page_entry;
}

std::optional<GPUVAddr> MemoryManager::FindFreeRange(std::size_t size, std::size_t align,
                                                     bool start_32bit_address) const {
    if (!align) {
        align = page_size;
    } else {
        align = Common::AlignUp(align, page_size);
    }

    u64 available_size{};
    GPUVAddr gpu_addr{start_32bit_address ? address_space_start_low : address_space_start};
    while (gpu_addr + available_size < address_space_size) {
        if (GetPageEntry(gpu_addr + available_size).IsUnmapped()) {
            available_size += page_size;

            if (available_size >= size) {
                return gpu_addr;
            }
        } else {
            gpu_addr += available_size + page_size;
            available_size = 0;

            const auto remainder{gpu_addr % align};
            if (remainder) {
                gpu_addr = (gpu_addr - remainder) + align;
            }
        }
    }

    return std::nullopt;
}

std::optional<VAddr> MemoryManager::GpuToCpuAddress(GPUVAddr gpu_addr) const {
    if (gpu_addr == 0) {
        return std::nullopt;
    }
    const auto page_entry{GetPageEntry(gpu_addr)};
    if (!page_entry.IsValid()) {
        return std::nullopt;
    }

    return page_entry.ToAddress() + (gpu_addr & page_mask);
}

std::optional<VAddr> MemoryManager::GpuToCpuAddress(GPUVAddr addr, std::size_t size) const {
    size_t page_index{addr >> page_bits};
    const size_t page_last{(addr + size + page_size - 1) >> page_bits};
    while (page_index < page_last) {
        const auto page_addr{GpuToCpuAddress(page_index << page_bits)};
        if (page_addr && *page_addr != 0) {
            return page_addr;
        }
        ++page_index;
    }
    return std::nullopt;
}

template <typename T>
T MemoryManager::Read(GPUVAddr addr) const {
    if (auto page_pointer{GetPointer(addr)}; page_pointer) {
        // NOTE: Avoid adding any extra logic to this fast-path block
        T value;
        std::memcpy(&value, page_pointer, sizeof(T));
        return value;
    }

    UNREACHABLE();

    return {};
}

template <typename T>
void MemoryManager::Write(GPUVAddr addr, T data) {
    if (auto page_pointer{GetPointer(addr)}; page_pointer) {
        // NOTE: Avoid adding any extra logic to this fast-path block
        std::memcpy(page_pointer, &data, sizeof(T));
        return;
    }

    UNREACHABLE();
}

template u8 MemoryManager::Read<u8>(GPUVAddr addr) const;
template u16 MemoryManager::Read<u16>(GPUVAddr addr) const;
template u32 MemoryManager::Read<u32>(GPUVAddr addr) const;
template u64 MemoryManager::Read<u64>(GPUVAddr addr) const;
template void MemoryManager::Write<u8>(GPUVAddr addr, u8 data);
template void MemoryManager::Write<u16>(GPUVAddr addr, u16 data);
template void MemoryManager::Write<u32>(GPUVAddr addr, u32 data);
template void MemoryManager::Write<u64>(GPUVAddr addr, u64 data);

u8* MemoryManager::GetPointer(GPUVAddr gpu_addr) {
    if (!GetPageEntry(gpu_addr).IsValid()) {
        return {};
    }

    const auto address{GpuToCpuAddress(gpu_addr)};
    if (!address) {
        return {};
    }

    return system.Memory().GetPointer(*address);
}

const u8* MemoryManager::GetPointer(GPUVAddr gpu_addr) const {
    if (!GetPageEntry(gpu_addr).IsValid()) {
        return {};
    }

    const auto address{GpuToCpuAddress(gpu_addr)};
    if (!address) {
        return {};
    }

    return system.Memory().GetPointer(*address);
}

size_t MemoryManager::BytesToMapEnd(GPUVAddr gpu_addr) const noexcept {
    auto it = std::ranges::upper_bound(map_ranges, gpu_addr, {}, &MapRange::first);
    --it;
    return it->second - (gpu_addr - it->first);
}

void MemoryManager::ReadBlock(GPUVAddr gpu_src_addr, void* dest_buffer, std::size_t size) const {
    std::size_t remaining_size{size};
    std::size_t page_index{gpu_src_addr >> page_bits};
    std::size_t page_offset{gpu_src_addr & page_mask};

    while (remaining_size > 0) {
        const std::size_t copy_amount{
            std::min(static_cast<std::size_t>(page_size) - page_offset, remaining_size)};

        if (const auto page_addr{GpuToCpuAddress(page_index << page_bits)}; page_addr) {
            const auto src_addr{*page_addr + page_offset};

            // Flush must happen on the rasterizer interface, such that memory is always synchronous
            // when it is read (even when in asynchronous GPU mode). Fixes Dead Cells title menu.
            rasterizer->FlushRegion(src_addr, copy_amount);
            system.Memory().ReadBlockUnsafe(src_addr, dest_buffer, copy_amount);
        }

        page_index++;
        page_offset = 0;
        dest_buffer = static_cast<u8*>(dest_buffer) + copy_amount;
        remaining_size -= copy_amount;
    }
}

void MemoryManager::ReadBlockUnsafe(GPUVAddr gpu_src_addr, void* dest_buffer,
                                    const std::size_t size) const {
    std::size_t remaining_size{size};
    std::size_t page_index{gpu_src_addr >> page_bits};
    std::size_t page_offset{gpu_src_addr & page_mask};

    while (remaining_size > 0) {
        const std::size_t copy_amount{
            std::min(static_cast<std::size_t>(page_size) - page_offset, remaining_size)};

        if (const auto page_addr{GpuToCpuAddress(page_index << page_bits)}; page_addr) {
            const auto src_addr{*page_addr + page_offset};
            system.Memory().ReadBlockUnsafe(src_addr, dest_buffer, copy_amount);
        } else {
            std::memset(dest_buffer, 0, copy_amount);
        }

        page_index++;
        page_offset = 0;
        dest_buffer = static_cast<u8*>(dest_buffer) + copy_amount;
        remaining_size -= copy_amount;
    }
}

void MemoryManager::WriteBlock(GPUVAddr gpu_dest_addr, const void* src_buffer, std::size_t size) {
    std::size_t remaining_size{size};
    std::size_t page_index{gpu_dest_addr >> page_bits};
    std::size_t page_offset{gpu_dest_addr & page_mask};

    while (remaining_size > 0) {
        const std::size_t copy_amount{
            std::min(static_cast<std::size_t>(page_size) - page_offset, remaining_size)};

        if (const auto page_addr{GpuToCpuAddress(page_index << page_bits)}; page_addr) {
            const auto dest_addr{*page_addr + page_offset};

            // Invalidate must happen on the rasterizer interface, such that memory is always
            // synchronous when it is written (even when in asynchronous GPU mode).
            rasterizer->InvalidateRegion(dest_addr, copy_amount);
            system.Memory().WriteBlockUnsafe(dest_addr, src_buffer, copy_amount);
        }

        page_index++;
        page_offset = 0;
        src_buffer = static_cast<const u8*>(src_buffer) + copy_amount;
        remaining_size -= copy_amount;
    }
}

void MemoryManager::WriteBlockUnsafe(GPUVAddr gpu_dest_addr, const void* src_buffer,
                                     std::size_t size) {
    std::size_t remaining_size{size};
    std::size_t page_index{gpu_dest_addr >> page_bits};
    std::size_t page_offset{gpu_dest_addr & page_mask};

    while (remaining_size > 0) {
        const std::size_t copy_amount{
            std::min(static_cast<std::size_t>(page_size) - page_offset, remaining_size)};

        if (const auto page_addr{GpuToCpuAddress(page_index << page_bits)}; page_addr) {
            const auto dest_addr{*page_addr + page_offset};
            system.Memory().WriteBlockUnsafe(dest_addr, src_buffer, copy_amount);
        }

        page_index++;
        page_offset = 0;
        src_buffer = static_cast<const u8*>(src_buffer) + copy_amount;
        remaining_size -= copy_amount;
    }
}

void MemoryManager::FlushRegion(GPUVAddr gpu_addr, size_t size) const {
    size_t remaining_size{size};
    size_t page_index{gpu_addr >> page_bits};
    size_t page_offset{gpu_addr & page_mask};
    while (remaining_size > 0) {
        const size_t num_bytes{std::min(page_size - page_offset, remaining_size)};
        if (const auto page_addr{GpuToCpuAddress(page_index << page_bits)}; page_addr) {
            rasterizer->FlushRegion(*page_addr + page_offset, num_bytes);
        }
        ++page_index;
        page_offset = 0;
        remaining_size -= num_bytes;
    }
}

void MemoryManager::CopyBlock(GPUVAddr gpu_dest_addr, GPUVAddr gpu_src_addr, std::size_t size) {
    std::vector<u8> tmp_buffer(size);
    ReadBlock(gpu_src_addr, tmp_buffer.data(), size);

    // The output block must be flushed in case it has data modified from the GPU.
    // Fixes NPC geometry in Zombie Panic in Wonderland DX
    FlushRegion(gpu_dest_addr, size);
    WriteBlock(gpu_dest_addr, tmp_buffer.data(), size);
}

bool MemoryManager::IsGranularRange(GPUVAddr gpu_addr, std::size_t size) const {
    const auto cpu_addr{GpuToCpuAddress(gpu_addr)};
    if (!cpu_addr) {
        return false;
    }
    const std::size_t page{(*cpu_addr & Core::Memory::PAGE_MASK) + size};
    return page <= Core::Memory::PAGE_SIZE;
}

bool MemoryManager::IsContinousRange(GPUVAddr gpu_addr, std::size_t size) const {
    size_t page_index{gpu_addr >> page_bits};
    const size_t page_last{(gpu_addr + size + page_size - 1) >> page_bits};
    std::optional<VAddr> old_page_addr{};
    while (page_index != page_last) {
        const auto page_addr{GpuToCpuAddress(page_index << page_bits)};
        if (!page_addr || *page_addr == 0) {
            return false;
        }
        if (old_page_addr) {
            if (*old_page_addr + page_size != *page_addr) {
                return false;
            }
        }
        old_page_addr = page_addr;
        ++page_index;
    }
    return true;
}

bool MemoryManager::IsFullyMappedRange(GPUVAddr gpu_addr, std::size_t size) const {
    size_t page_index{gpu_addr >> page_bits};
    const size_t page_last{(gpu_addr + size + page_size - 1) >> page_bits};
    while (page_index < page_last) {
        if (!page_table[page_index].IsValid() || page_table[page_index].ToAddress() == 0) {
            return false;
        }
        ++page_index;
    }
    return true;
}

std::vector<std::pair<GPUVAddr, std::size_t>> MemoryManager::GetSubmappedRange(
    GPUVAddr gpu_addr, std::size_t size) const {
    std::vector<std::pair<GPUVAddr, std::size_t>> result{};
    size_t page_index{gpu_addr >> page_bits};
    size_t remaining_size{size};
    size_t page_offset{gpu_addr & page_mask};
    std::optional<std::pair<GPUVAddr, std::size_t>> last_segment{};
    std::optional<VAddr> old_page_addr{};
    const auto extend_size = [this, &last_segment, &page_index](std::size_t bytes) {
        if (!last_segment) {
            GPUVAddr new_base_addr = page_index << page_bits;
            last_segment = {new_base_addr, bytes};
        } else {
            last_segment->second += bytes;
        }
    };
    const auto split = [this, &last_segment, &result] {
        if (last_segment) {
            result.push_back(*last_segment);
            last_segment = std::nullopt;
        }
    };
    while (remaining_size > 0) {
        const size_t num_bytes{std::min(page_size - page_offset, remaining_size)};
        const auto page_addr{GpuToCpuAddress(page_index << page_bits)};
        if (!page_addr) {
            split();
        } else if (old_page_addr) {
            if (*old_page_addr + page_size != *page_addr) {
                split();
            }
            extend_size(num_bytes);
        } else {
            extend_size(num_bytes);
        }
        ++page_index;
        page_offset = 0;
        remaining_size -= num_bytes;
    }
    split();
    return result;
}

} // namespace Tegra
