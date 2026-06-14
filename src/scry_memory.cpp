#include <scry/scry_memory.hpp>
#include <mimalloc.h>
#include <libassert/assert.hpp>

namespace Scry {
namespace Memory {

bool IsUsingMimalloc(const void* ptr) {
    DEBUG_ASSERT(ptr != nullptr);
    if (ptr == nullptr) {
        return false;
    }
    const bool res = mi_is_in_heap_region(ptr);
    return res;
}

void* AllocInDll(size_t size) {
    DEBUG_ASSERT(size > 0);
    DEBUG_ASSERT(size < 1024 * 1024 * 1024);
    void* ptr = ::operator new(size);
    DEBUG_ASSERT(ptr != nullptr);
    return ptr;
}

void FreeInDll(void* ptr) {
    if (ptr == nullptr) {
        return;
    }
    ::operator delete(ptr);
}

void ScryArenaInit(Arena* arena, void* backing_memory, size_t size) {
    DEBUG_ASSERT(arena != nullptr);
    DEBUG_ASSERT(backing_memory != nullptr);
    DEBUG_ASSERT(size > 0);

    arena->backing_memory = static_cast<uint8_t*>(backing_memory);
    arena->total_size = size;
    arena->offset = 0;
}

void* ScryArenaAllocate(Arena* arena, size_t size, size_t alignment) {
    DEBUG_ASSERT(arena != nullptr);
    DEBUG_ASSERT(size > 0);
    DEBUG_ASSERT(alignment > 0);

    if (arena->backing_memory == nullptr || size == 0) {
        return nullptr;
    }

    const uintptr_t current_ptr = reinterpret_cast<uintptr_t>(arena->backing_memory + arena->offset);
    const uintptr_t aligned_ptr = (current_ptr + (alignment - 1)) & ~(alignment - 1);
    const size_t aligned_offset = aligned_ptr - reinterpret_cast<uintptr_t>(arena->backing_memory);

    if (aligned_offset + size > arena->total_size) {
        return nullptr;
    }

    arena->offset = aligned_offset + size;
    void* result = reinterpret_cast<void*>(aligned_ptr);
    DEBUG_ASSERT(result != nullptr);
    return result;
}

void ScryArenaReset(Arena* arena) {
    DEBUG_ASSERT(arena != nullptr);
    arena->offset = 0;
}

static size_t AlignSize(size_t size, size_t alignment) {
    DEBUG_ASSERT(alignment > 0);
    DEBUG_ASSERT((alignment & (alignment - 1)) == 0);
    return (size + (alignment - 1)) & ~(alignment - 1);
}

size_t ScryPoolGetRequiredSize(size_t block_size, uint32_t capacity) {
    DEBUG_ASSERT(block_size > 0);
    DEBUG_ASSERT(capacity > 0);
    
    const size_t data_size = AlignSize(block_size * capacity, alignof(uint32_t));
    const size_t next_free_size = AlignSize(sizeof(uint32_t) * capacity, alignof(int8_t));
    const size_t states_size = AlignSize(sizeof(int8_t) * capacity, sizeof(void*));
    return data_size + next_free_size + states_size;
}

void ScryPoolInit(PoolAllocator* pool, void* memory, size_t memory_size, size_t block_size, uint32_t capacity) {
    DEBUG_ASSERT(pool != nullptr);
    DEBUG_ASSERT(memory != nullptr);
    DEBUG_ASSERT(memory_size > 0);
    DEBUG_ASSERT(block_size > 0);
    DEBUG_ASSERT(capacity > 0);

    const size_t required = ScryPoolGetRequiredSize(block_size, capacity);
    DEBUG_ASSERT(memory_size >= required);
    if (memory_size < required) {
        return;
    }

    const size_t data_size = AlignSize(block_size * capacity, alignof(uint32_t));
    const size_t next_free_size = AlignSize(sizeof(uint32_t) * capacity, alignof(int8_t));

    uint8_t* ptr = static_cast<uint8_t*>(memory);
    pool->data = ptr;
    pool->next_free = reinterpret_cast<uint32_t*>(ptr + data_size);
    pool->states = reinterpret_cast<int8_t*>(ptr + data_size + next_free_size);

    pool->block_size = block_size;
    pool->capacity = capacity;
    pool->first_free = 0;
    pool->active_count = 0;

    for (uint32_t i = 0; i < capacity; ++i) {
        pool->states[i] = 0; // Free
        if (i < capacity - 1) {
            pool->next_free[i] = i + 1;
        } else {
            pool->next_free[i] = 0xFFFFFFFF; // INVALID_INDEX
        }
    }
}

uint32_t ScryPoolAllocate(PoolAllocator* pool) {
    DEBUG_ASSERT(pool != nullptr);
    DEBUG_ASSERT(pool->capacity > 0);

    if (pool->first_free == 0xFFFFFFFF) {
        return 0xFFFFFFFF;
    }

    const uint32_t index = pool->first_free;
    pool->first_free = pool->next_free[index];
    pool->states[index] = 1; // Active
    pool->active_count++;

    return index;
}

void ScryPoolFree(PoolAllocator* pool, uint32_t index) {
    DEBUG_ASSERT(pool != nullptr);
    DEBUG_ASSERT(pool->capacity > 0);

    if (index >= pool->capacity) {
        return;
    }
    if (pool->states[index] == 0) {
        return;
    }

    pool->states[index] = 0;
    pool->next_free[index] = pool->first_free;
    pool->first_free = index;
    pool->active_count--;
}

void ScryPoolReset(PoolAllocator* pool) {
    DEBUG_ASSERT(pool != nullptr);
    DEBUG_ASSERT(pool->capacity > 0);

    pool->first_free = 0;
    pool->active_count = 0;
    for (uint32_t i = 0; i < pool->capacity; ++i) {
        pool->states[i] = 0;
        if (i < pool->capacity - 1) {
            pool->next_free[i] = i + 1;
        } else {
            pool->next_free[i] = 0xFFFFFFFF;
        }
    }
}

} // namespace Memory
} // namespace Scry

// Global operator new/delete overrides to use mimalloc
void* operator new(size_t size) {
    DEBUG_ASSERT(size > 0);
    void* ptr = mi_malloc(size);
    return ptr;
}

void* operator new[](size_t size) {
    DEBUG_ASSERT(size > 0);
    void* ptr = mi_malloc(size);
    return ptr;
}

void operator delete(void* p) noexcept {
    mi_free(p);
}

void operator delete[](void* p) noexcept {
    mi_free(p);
}

void operator delete(void* p, size_t size) noexcept {
    DEBUG_ASSERT(size > 0);
    mi_free(p);
}

void operator delete[](void* p, size_t size) noexcept {
    DEBUG_ASSERT(size > 0);
    mi_free(p);
}

void* operator new(size_t size, const std::nothrow_t&) noexcept {
    DEBUG_ASSERT(size > 0);
    void* ptr = mi_malloc(size);
    return ptr;
}

void* operator new[](size_t size, const std::nothrow_t&) noexcept {
    DEBUG_ASSERT(size > 0);
    void* ptr = mi_malloc(size);
    return ptr;
}

void operator delete(void* p, const std::nothrow_t&) noexcept {
    mi_free(p);
}

void operator delete[](void* p, const std::nothrow_t&) noexcept {
    mi_free(p);
}
