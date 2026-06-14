#pragma once
#include <engine/engine.h>
#include <cstddef>
#include <cstdint>
#include <libassert/assert.hpp>

namespace Engine {
namespace Memory {

// Check if a pointer is managed by the mimalloc heap.
ENGINE_API bool IsUsingMimalloc(const void* ptr);

// Allocate and free memory inside the DLL to test its allocator override.
ENGINE_API void* AllocInDll(size_t size);
ENGINE_API void FreeInDll(void* ptr);

// Pure flat Arena struct
struct ENGINE_API Arena {
    uint8_t* backing_memory = nullptr; // 8 bytes
    size_t total_size = 0;             // 8 bytes
    size_t offset = 0;                 // 8 bytes
};

// Standalone functions for Arena
ENGINE_API void ArenaInit(Arena* arena, void* backing_memory, size_t size);
ENGINE_API void* ArenaAllocate(Arena* arena, size_t size, size_t alignment = sizeof(void*));
ENGINE_API void ArenaReset(Arena* arena);

inline size_t ArenaGetUsedMemory(const Arena* arena) {
    DEBUG_ASSERT(arena != nullptr);
    return arena->offset;
}

inline size_t ArenaGetTotalSize(const Arena* arena) {
    DEBUG_ASSERT(arena != nullptr);
    return arena->total_size;
}

inline size_t ArenaGetRemainingMemory(const Arena* arena) {
    DEBUG_ASSERT(arena != nullptr);
    DEBUG_ASSERT(arena->total_size >= arena->offset);
    return arena->total_size - arena->offset;
}

// Pure flat PoolAllocator struct
struct ENGINE_API PoolAllocator {
    void* data = nullptr;                // 8 bytes
    uint32_t* next_free = nullptr;       // 8 bytes
    int8_t* states = nullptr;            // 8 bytes
    size_t block_size = 0;               // 8 bytes
    uint32_t capacity = 0;               // 4 bytes
    uint32_t first_free = 0xFFFFFFFF;    // 4 bytes
    uint32_t active_count = 0;           // 4 bytes
};

// Standalone functions for PoolAllocator
ENGINE_API size_t PoolGetRequiredSize(size_t block_size, uint32_t capacity);
ENGINE_API void PoolInit(PoolAllocator* pool, void* memory, size_t memory_size, size_t block_size, uint32_t capacity);
ENGINE_API uint32_t PoolAllocate(PoolAllocator* pool);
ENGINE_API void PoolFree(PoolAllocator* pool, uint32_t index);

inline void* PoolGet(PoolAllocator* pool, uint32_t index) {
    DEBUG_ASSERT(pool != nullptr);
    DEBUG_ASSERT(pool->capacity > 0);
    if (index >= pool->capacity || pool->states[index] == 0) {
        return nullptr;
    }
    return static_cast<uint8_t*>(pool->data) + (index * pool->block_size);
}

inline const void* PoolGetConst(const PoolAllocator* pool, uint32_t index) {
    DEBUG_ASSERT(pool != nullptr);
    DEBUG_ASSERT(pool->capacity > 0);
    if (index >= pool->capacity || pool->states[index] == 0) {
        return nullptr;
    }
    return static_cast<const uint8_t*>(pool->data) + (index * pool->block_size);
}

ENGINE_API void PoolReset(PoolAllocator* pool);

inline size_t PoolGetCapacity(const PoolAllocator* pool) {
    DEBUG_ASSERT(pool != nullptr);
    return pool->capacity;
}

inline size_t PoolGetActiveCount(const PoolAllocator* pool) {
    DEBUG_ASSERT(pool != nullptr);
    return pool->active_count;
}

} // namespace Memory
} // namespace Engine
