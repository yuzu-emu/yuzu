// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <optional>
#include <vector>

#include "common/common_types.h"

namespace VideoCore {
class RasterizerInterface;
}

namespace Core {
class System;
}

namespace Tegra {

class PageEntry final {
public:
    enum class State : u32 {
        Unmapped = static_cast<u32>(-1),
        Allocated = static_cast<u32>(-2),
    };

    constexpr PageEntry() = default;
    constexpr PageEntry(State state_) : state{state_} {}
    constexpr PageEntry(VAddr addr) : state{static_cast<State>(addr >> ShiftBits)} {}

    [[nodiscard]] constexpr bool IsUnmapped() const {
        return state == State::Unmapped;
    }

    [[nodiscard]] constexpr bool IsAllocated() const {
        return state == State::Allocated;
    }

    [[nodiscard]] constexpr bool IsValid() const {
        return !IsUnmapped() && !IsAllocated();
    }

    [[nodiscard]] constexpr VAddr ToAddress() const {
        if (!IsValid()) {
            return {};
        }

        return static_cast<VAddr>(state) << ShiftBits;
    }

    [[nodiscard]] constexpr PageEntry operator+(u64 offset) const {
        // If this is a reserved value, offsets do not apply
        if (!IsValid()) {
            return *this;
        }
        return PageEntry{(static_cast<VAddr>(state) << ShiftBits) + offset};
    }

private:
    static constexpr std::size_t ShiftBits{12};

    State state{State::Unmapped};
};
static_assert(sizeof(PageEntry) == 4, "PageEntry is too large");

class MemoryManager final {
public:
    explicit MemoryManager(Core::System& system_);
    ~MemoryManager();

    /// Binds a renderer to the memory manager.
    void BindRasterizer(VideoCore::RasterizerInterface* rasterizer);

    [[nodiscard]] std::optional<VAddr> GpuToCpuAddress(GPUVAddr addr) const;

    [[nodiscard]] std::optional<VAddr> GpuToCpuAddress(GPUVAddr addr, std::size_t size) const;

    template <typename T>
    [[nodiscard]] T Read(GPUVAddr addr) const;

    template <typename T>
    void Write(GPUVAddr addr, T data);

    [[nodiscard]] u8* GetPointer(GPUVAddr addr);
    [[nodiscard]] const u8* GetPointer(GPUVAddr addr) const;

    /// Returns the number of bytes until the end of the memory map containing the given GPU address
    [[nodiscard]] size_t BytesToMapEnd(GPUVAddr gpu_addr) const noexcept;

    /**
     * ReadBlock and WriteBlock are full read and write operations over virtual
     * GPU Memory. It's important to use these when GPU memory may not be continuous
     * in the Host Memory counterpart. Note: This functions cause Host GPU Memory
     * Flushes and Invalidations, respectively to each operation.
     */
    void ReadBlock(GPUVAddr gpu_src_addr, void* dest_buffer, std::size_t size) const;
    void WriteBlock(GPUVAddr gpu_dest_addr, const void* src_buffer, std::size_t size);
    void CopyBlock(GPUVAddr gpu_dest_addr, GPUVAddr gpu_src_addr, std::size_t size);

    /**
     * ReadBlockUnsafe and WriteBlockUnsafe are special versions of ReadBlock and
     * WriteBlock respectively. In this versions, no flushing or invalidation is actually
     * done and their performance is similar to a memcpy. This functions can be used
     * on either of this 2 scenarios instead of their safe counterpart:
     * - Memory which is sure to never be represented in the Host GPU.
     * - Memory Managed by a Cache Manager. Example: Texture Flushing should use
     * WriteBlockUnsafe instead of WriteBlock since it shouldn't invalidate the texture
     * being flushed.
     */
    void ReadBlockUnsafe(GPUVAddr gpu_src_addr, void* dest_buffer, std::size_t size) const;
    void WriteBlockUnsafe(GPUVAddr gpu_dest_addr, const void* src_buffer, std::size_t size);

    /**
     * Checks if a gpu region can be simply read with a pointer.
     */
    [[nodiscard]] bool IsGranularRange(GPUVAddr gpu_addr, std::size_t size) const;

    /**
     * Checks if a gpu region is mapped by a single range of cpu addresses.
     */
    [[nodiscard]] bool IsContinousRange(GPUVAddr gpu_addr, std::size_t size) const;

    /**
     * Checks if a gpu region is mapped entirely.
     */
    [[nodiscard]] bool IsFullyMappedRange(GPUVAddr gpu_addr, std::size_t size) const;

    /**
     * Returns a vector with all the subranges of cpu addresses mapped beneath.
     * if the region is continous, a single pair will be returned. If it's unmapped, an empty vector
     * will be returned;
     */
    std::vector<std::pair<GPUVAddr, std::size_t>> GetSubmappedRange(GPUVAddr gpu_addr,
                                                                    std::size_t size) const;

    [[nodiscard]] GPUVAddr Map(VAddr cpu_addr, GPUVAddr gpu_addr, std::size_t size);
    [[nodiscard]] GPUVAddr MapAllocate(VAddr cpu_addr, std::size_t size, std::size_t align);
    [[nodiscard]] GPUVAddr MapAllocate32(VAddr cpu_addr, std::size_t size);
    [[nodiscard]] std::optional<GPUVAddr> AllocateFixed(GPUVAddr gpu_addr, std::size_t size);
    [[nodiscard]] GPUVAddr Allocate(std::size_t size, std::size_t align);
    void Unmap(GPUVAddr gpu_addr, std::size_t size);

    /**
     * Some Decoded NVDEC frames require that texture cache does not get invalidated.
     * UnmapVicFrame defers the texture cache invalidation until the stream ends
     * by invoking InvalidateQueuedCaches to invalidate all frame texture caches.
     */
    void UnmapVicFrame(GPUVAddr gpu_addr, std::size_t size);
    void InvalidateQueuedCaches();

private:
    [[nodiscard]] PageEntry GetPageEntry(GPUVAddr gpu_addr) const;
    void SetPageEntry(GPUVAddr gpu_addr, PageEntry page_entry, std::size_t size = page_size);
    GPUVAddr UpdateRange(GPUVAddr gpu_addr, PageEntry page_entry, std::size_t size);
    [[nodiscard]] std::optional<GPUVAddr> FindFreeRange(std::size_t size, std::size_t align,
                                                        bool start_32bit_address = false) const;

    void TryLockPage(PageEntry page_entry, std::size_t size);
    void TryUnlockPage(PageEntry page_entry, std::size_t size);

    void FlushRegion(GPUVAddr gpu_addr, size_t size) const;

    [[nodiscard]] static constexpr std::size_t PageEntryIndex(GPUVAddr gpu_addr) {
        return (gpu_addr >> page_bits) & page_table_mask;
    }

    static constexpr u64 address_space_size = 1ULL << 40;
    static constexpr u64 address_space_start = 1ULL << 32;
    static constexpr u64 address_space_start_low = 1ULL << 16;
    static constexpr u64 page_bits{16};
    static constexpr u64 page_size{1 << page_bits};
    static constexpr u64 page_mask{page_size - 1};
    static constexpr u64 page_table_bits{24};
    static constexpr u64 page_table_size{1 << page_table_bits};
    static constexpr u64 page_table_mask{page_table_size - 1};

    Core::System& system;

    VideoCore::RasterizerInterface* rasterizer = nullptr;

    std::vector<PageEntry> page_table;

    using MapRange = std::pair<GPUVAddr, size_t>;
    std::vector<MapRange> map_ranges;

    std::vector<std::pair<VAddr, std::size_t>> cache_invalidate_queue;
};

} // namespace Tegra
