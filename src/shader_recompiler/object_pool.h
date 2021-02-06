// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <type_traits>
#include <utility>

namespace Shader {

template <typename T, size_t chunk_size = 8192>
requires std::is_destructible_v<T> class ObjectPool {
public:
    ~ObjectPool() {
        std::unique_ptr<Chunk> tree_owner;
        Chunk* chunk{&root};
        while (chunk) {
            for (size_t obj_id = chunk->free_objects; obj_id < chunk_size; ++obj_id) {
                chunk->storage[obj_id].object.~T();
            }
            tree_owner = std::move(chunk->next);
            chunk = tree_owner.get();
        }
    }

    template <typename... Args>
    requires std::is_constructible_v<T, Args...> [[nodiscard]] T* Create(Args&&... args) {
        return std::construct_at(Memory(), std::forward<Args>(args)...);
    }

    void ReleaseContents() {
        Chunk* chunk{&root};
        while (chunk) {
            if (chunk->free_objects == chunk_size) {
                break;
            }
            for (; chunk->free_objects < chunk_size; ++chunk->free_objects) {
                chunk->storage[chunk->free_objects].object.~T();
            }
            chunk = chunk->next.get();
        }
        node = &root;
    }

private:
    struct NonTrivialDummy {
        NonTrivialDummy() noexcept {}
    };

    union Storage {
        Storage() noexcept {}
        ~Storage() noexcept {}

        NonTrivialDummy dummy{};
        T object;
    };

    struct Chunk {
        size_t free_objects = chunk_size;
        std::array<Storage, chunk_size> storage;
        std::unique_ptr<Chunk> next;
    };

    [[nodiscard]] T* Memory() {
        Chunk* const chunk{FreeChunk()};
        return &chunk->storage[--chunk->free_objects].object;
    }

    [[nodiscard]] Chunk* FreeChunk() {
        if (node->free_objects > 0) {
            return node;
        }
        if (node->next) {
            node = node->next.get();
            return node;
        }
        node->next = std::make_unique<Chunk>();
        node = node->next.get();
        return node;
    }

    Chunk* node{&root};
    Chunk root;
};

} // namespace Shader
