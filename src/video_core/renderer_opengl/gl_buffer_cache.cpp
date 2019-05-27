// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <memory>
#include <utility>

#include "common/alignment.h"
#include "core/core.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_opengl/gl_buffer_cache.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"

namespace OpenGL {

CachedBufferEntry::CachedBufferEntry(VAddr cpu_addr, u8* host_ptr, std::size_t size,
                                     std::size_t alignment, GLuint buffer, GLintptr offset)
    : RasterizerCacheObject{host_ptr}, cpu_addr{cpu_addr}, size{size}, alignment{alignment},
      buffer{buffer}, offset{offset} {}

OGLBufferCache::OGLBufferCache(RasterizerOpenGL& rasterizer, std::size_t size)
    : RasterizerCache{rasterizer}, stream_buffer(size, true) {}

std::pair<GLuint, GLintptr> OGLBufferCache::UploadMemory(GPUVAddr gpu_addr, std::size_t size,
                                                         std::size_t alignment, bool cache) {
    std::lock_guard lock{mutex};

    auto& memory_manager = Core::System::GetInstance().GPU().MemoryManager();

    const auto& host_ptr{memory_manager.GetPointer(gpu_addr)};
    if (!host_ptr) {
        // Return a dummy buffer when host_ptr is invalid.
        return {0, 0};
    }

    // Cache management is a big overhead, so only cache entries with a given size.
    // TODO: Figure out which size is the best for given games.
    cache &= size >= 2048;

    if (cache) {
        if (auto entry = TryGet(host_ptr); entry) {
            if (entry->GetSize() >= size && entry->GetAlignment() == alignment) {
                return {entry->GetBuffer(), entry->GetOffset()};
            }
            Unregister(entry);
        }
    }

    AlignBuffer(alignment);
    const GLintptr uploaded_offset = buffer_offset;

    std::memcpy(buffer_ptr, host_ptr, size);
    buffer_ptr += size;
    buffer_offset += size;

    const GLuint buffer = stream_buffer.GetHandle();
    if (cache) {
        const VAddr cpu_addr = *memory_manager.GpuToCpuAddress(gpu_addr);
        Register(std::make_shared<CachedBufferEntry>(cpu_addr, host_ptr, size, alignment, buffer,
                                                     uploaded_offset));
    }

    return {buffer, uploaded_offset};
}

std::pair<GLuint, GLintptr> OGLBufferCache::UploadHostMemory(const void* raw_pointer,
                                                             std::size_t size,
                                                             std::size_t alignment) {
    std::lock_guard lock{mutex};
    AlignBuffer(alignment);
    std::memcpy(buffer_ptr, raw_pointer, size);
    const GLintptr uploaded_offset = buffer_offset;

    buffer_ptr += size;
    buffer_offset += size;
    return {stream_buffer.GetHandle(), uploaded_offset};
}

bool OGLBufferCache::Map(std::size_t max_size) {
    bool invalidate;
    std::tie(buffer_ptr, buffer_offset_base, invalidate) =
        stream_buffer.Map(static_cast<GLsizeiptr>(max_size), 4);
    buffer_offset = buffer_offset_base;

    if (invalidate) {
        InvalidateAll();
    }
    return invalidate;
}

void OGLBufferCache::Unmap() {
    stream_buffer.Unmap(buffer_offset - buffer_offset_base);
}

void OGLBufferCache::AlignBuffer(std::size_t alignment) {
    // Align the offset, not the mapped pointer
    const GLintptr offset_aligned =
        static_cast<GLintptr>(Common::AlignUp(static_cast<std::size_t>(buffer_offset), alignment));
    buffer_ptr += offset_aligned - buffer_offset;
    buffer_offset = offset_aligned;
}

} // namespace OpenGL
