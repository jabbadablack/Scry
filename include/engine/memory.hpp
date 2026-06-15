#pragma once
#include <engine/engine.h>
#include <cstddef>
#include <cstdint>
#include <cassert>

namespace Engine {
namespace Memory {

struct ENGINE_API Arena {
    uint8_t* backing_memory = nullptr;
    size_t total_size = 0;
    size_t offset = 0;
};

ENGINE_API void  ArenaInit(Arena* arena, void* backing_memory, size_t size);
ENGINE_API void* ArenaAllocate(Arena* arena, size_t size, size_t alignment = sizeof(void*));
ENGINE_API void  ArenaReset(Arena* arena);

inline size_t ArenaGetUsedMemory(const Arena* arena) {
    assert(arena != nullptr);
    return arena->offset;
}

inline size_t ArenaGetTotalSize(const Arena* arena) {
    assert(arena != nullptr);
    return arena->total_size;
}

inline size_t ArenaGetRemainingMemory(const Arena* arena) {
    assert(arena != nullptr);
    assert(arena->total_size >= arena->offset);
    return arena->total_size - arena->offset;
}

struct ENGINE_API PoolAllocator {
    void*     data        = nullptr;
    uint32_t* next_free   = nullptr;
    int8_t*   states      = nullptr;
    size_t    block_size  = 0;
    uint32_t  capacity    = 0;
    uint32_t  first_free  = 0xFFFFFFFF;
    uint32_t  active_count = 0;
};

ENGINE_API size_t   PoolGetRequiredSize(size_t block_size, uint32_t capacity);
ENGINE_API void     PoolInit(PoolAllocator* pool, void* memory, size_t memory_size, size_t block_size, uint32_t capacity);
ENGINE_API uint32_t PoolAllocate(PoolAllocator* pool);
ENGINE_API void     PoolFree(PoolAllocator* pool, uint32_t index);
ENGINE_API void     PoolReset(PoolAllocator* pool);

inline void* PoolGet(PoolAllocator* pool, uint32_t index) {
    assert(pool != nullptr);
    if (index >= pool->capacity || pool->states[index] == 0) return nullptr;
    return static_cast<uint8_t*>(pool->data) + (index * pool->block_size);
}

inline const void* PoolGetConst(const PoolAllocator* pool, uint32_t index) {
    assert(pool != nullptr);
    if (index >= pool->capacity || pool->states[index] == 0) return nullptr;
    return static_cast<const uint8_t*>(pool->data) + (index * pool->block_size);
}

inline size_t PoolGetCapacity(const PoolAllocator* pool) {
    assert(pool != nullptr);
    return pool->capacity;
}

inline size_t PoolGetActiveCount(const PoolAllocator* pool) {
    assert(pool != nullptr);
    return pool->active_count;
}

} // namespace Memory
} // namespace Engine
