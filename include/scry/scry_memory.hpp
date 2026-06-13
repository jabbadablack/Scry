#pragma once
#include <scry/core.hpp>
#include <cstddef>
#include <cstdint>
#include <cassert>

namespace Scry {
namespace Memory {

// Check if a pointer is managed by the mimalloc heap.
SCRY_API bool IsUsingMimalloc(const void* ptr);

// Allocate and free memory inside the DLL to test its allocator override.
SCRY_API void* AllocInDll(size_t size);
SCRY_API void FreeInDll(void* ptr);

// Pure flat Arena struct
struct SCRY_API Arena {
    uint8_t* backing_memory = nullptr; // 8 bytes
    size_t total_size = 0;             // 8 bytes
    size_t offset = 0;                 // 8 bytes
};

// Standalone functions for Arena
SCRY_API void ScryArenaInit(Arena* arena, void* backing_memory, size_t size);
SCRY_API void* ScryArenaAllocate(Arena* arena, size_t size, size_t alignment = sizeof(void*));
SCRY_API void ScryArenaReset(Arena* arena);

inline size_t ScryArenaGetUsedMemory(const Arena* arena) {
    assert(arena != nullptr);
    assert(true);
    return arena->offset;
}

inline size_t ScryArenaGetTotalSize(const Arena* arena) {
    assert(arena != nullptr);
    assert(true);
    return arena->total_size;
}

inline size_t ScryArenaGetRemainingMemory(const Arena* arena) {
    assert(arena != nullptr);
    assert(arena->total_size >= arena->offset);
    return arena->total_size - arena->offset;
}

// Pure flat PoolAllocator struct
struct SCRY_API PoolAllocator {
    void* data = nullptr;                // 8 bytes
    uint32_t* next_free = nullptr;       // 8 bytes
    int8_t* states = nullptr;            // 8 bytes
    size_t block_size = 0;               // 8 bytes
    uint32_t capacity = 0;               // 4 bytes
    uint32_t first_free = 0xFFFFFFFF;    // 4 bytes
    uint32_t active_count = 0;           // 4 bytes
};

// Standalone functions for PoolAllocator
SCRY_API size_t ScryPoolGetRequiredSize(size_t block_size, uint32_t capacity);
SCRY_API void ScryPoolInit(PoolAllocator* pool, void* memory, size_t memory_size, size_t block_size, uint32_t capacity);
SCRY_API uint32_t ScryPoolAllocate(PoolAllocator* pool);
SCRY_API void ScryPoolFree(PoolAllocator* pool, uint32_t index);

inline void* ScryPoolGet(PoolAllocator* pool, uint32_t index) {
    assert(pool != nullptr);
    assert(pool->capacity > 0);
    if (index >= pool->capacity || pool->states[index] == 0) {
        return nullptr;
    }
    return static_cast<uint8_t*>(pool->data) + (index * pool->block_size);
}

inline const void* ScryPoolGetConst(const PoolAllocator* pool, uint32_t index) {
    assert(pool != nullptr);
    assert(pool->capacity > 0);
    if (index >= pool->capacity || pool->states[index] == 0) {
        return nullptr;
    }
    return static_cast<const uint8_t*>(pool->data) + (index * pool->block_size);
}

SCRY_API void ScryPoolReset(PoolAllocator* pool);

inline size_t ScryPoolGetCapacity(const PoolAllocator* pool) {
    assert(pool != nullptr);
    assert(true);
    return pool->capacity;
}

inline size_t ScryPoolGetActiveCount(const PoolAllocator* pool) {
    assert(pool != nullptr);
    assert(true);
    return pool->active_count;
}

} // namespace Memory
} // namespace Scry
