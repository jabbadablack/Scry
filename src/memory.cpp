#include <engine/memory.hpp>
#include <cassert>
#include <new>

namespace Engine {
namespace Memory {

void ArenaInit(Arena* arena, void* backing_memory, size_t size) {
    assert(arena != nullptr);
    assert(backing_memory != nullptr);
    assert(size > 0);
    arena->backing_memory = static_cast<uint8_t*>(backing_memory);
    arena->total_size = size;
    arena->offset = 0;
}

void* ArenaAllocate(Arena* arena, size_t size, size_t alignment) {
    assert(arena != nullptr);
    assert(size > 0);
    assert(alignment > 0);
    if (!arena->backing_memory || size == 0) return nullptr;

    const uintptr_t cur = reinterpret_cast<uintptr_t>(arena->backing_memory + arena->offset);
    const uintptr_t aligned = (cur + (alignment - 1)) & ~(alignment - 1);
    const size_t aligned_offset = aligned - reinterpret_cast<uintptr_t>(arena->backing_memory);

    if (aligned_offset + size > arena->total_size) return nullptr;
    arena->offset = aligned_offset + size;
    return reinterpret_cast<void*>(aligned);
}

void ArenaReset(Arena* arena) {
    assert(arena != nullptr);
    arena->offset = 0;
}

static size_t AlignSize(size_t size, size_t alignment) {
    return (size + (alignment - 1)) & ~(alignment - 1);
}

size_t PoolGetRequiredSize(size_t block_size, uint32_t capacity) {
    const size_t data_size      = AlignSize(block_size * capacity, alignof(uint32_t));
    const size_t next_free_size = AlignSize(sizeof(uint32_t) * capacity, alignof(int8_t));
    const size_t states_size    = AlignSize(sizeof(int8_t) * capacity, sizeof(void*));
    return data_size + next_free_size + states_size;
}

void PoolInit(PoolAllocator* pool, void* memory, size_t memory_size, size_t block_size, uint32_t capacity) {
    assert(pool && memory && memory_size > 0 && block_size > 0 && capacity > 0);
    assert(memory_size >= PoolGetRequiredSize(block_size, capacity));

    const size_t data_size      = AlignSize(block_size * capacity, alignof(uint32_t));
    const size_t next_free_size = AlignSize(sizeof(uint32_t) * capacity, alignof(int8_t));

    uint8_t* ptr = static_cast<uint8_t*>(memory);
    pool->data       = ptr;
    pool->next_free  = reinterpret_cast<uint32_t*>(ptr + data_size);
    pool->states     = reinterpret_cast<int8_t*>(ptr + data_size + next_free_size);
    pool->block_size  = block_size;
    pool->capacity    = capacity;
    pool->first_free  = 0;
    pool->active_count = 0;

    for (uint32_t i = 0; i < capacity; ++i) {
        pool->states[i] = 0;
        pool->next_free[i] = (i < capacity - 1) ? i + 1 : 0xFFFFFFFF;
    }
}

uint32_t PoolAllocate(PoolAllocator* pool) {
    assert(pool != nullptr);
    if (pool->first_free == 0xFFFFFFFF) return 0xFFFFFFFF;
    const uint32_t index = pool->first_free;
    pool->first_free = pool->next_free[index];
    pool->states[index] = 1;
    ++pool->active_count;
    return index;
}

void PoolFree(PoolAllocator* pool, uint32_t index) {
    assert(pool != nullptr);
    if (index >= pool->capacity || pool->states[index] == 0) return;
    pool->states[index] = 0;
    pool->next_free[index] = pool->first_free;
    pool->first_free = index;
    --pool->active_count;
}

void PoolReset(PoolAllocator* pool) {
    assert(pool != nullptr);
    pool->first_free = 0;
    pool->active_count = 0;
    for (uint32_t i = 0; i < pool->capacity; ++i) {
        pool->states[i] = 0;
        pool->next_free[i] = (i < pool->capacity - 1) ? i + 1 : 0xFFFFFFFF;
    }
}

} // namespace Memory
} // namespace Engine
