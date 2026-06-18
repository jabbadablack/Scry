#include <engine/memory.h>
#include <engine/engine.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ScryArena g_FrameArena = {0};

void* Arena_Alloc(size_t size) {
    return ScryMemory_ArenaAllocate(&g_FrameArena, size, 16u);
}

void Arena_Reset(void) {
    ScryMemory_ArenaReset(&g_FrameArena);
}

void ScryMemory_ArenaInit(ScryArena* arena, void* backing_memory, size_t size) {
    assert(arena != NULL);
    assert(backing_memory != NULL);
    arena->backing_memory = (uint8_t*)backing_memory;
    arena->total_size = size;
    arena->offset = 0;
}

void* ScryMemory_ArenaAllocate(ScryArena* arena, size_t size, size_t alignment) {
    assert(arena != NULL);
    const size_t current_ptr = (size_t)arena->backing_memory + arena->offset;
    const size_t offset = (current_ptr + (alignment - 1)) & ~(alignment - 1);
    const size_t relative_offset = offset - (size_t)arena->backing_memory;

    if (relative_offset + size > arena->total_size) return NULL;

    arena->offset = relative_offset + size;
    return (void*)offset;
}

void ScryMemory_ArenaReset(ScryArena* arena) {
    assert(arena != NULL);
    arena->offset = 0;
}

size_t ScryMemory_PoolGetRequiredSize(size_t block_size, uint32_t capacity) {
    // block_size * capacity + (uint32_t * capacity) + (int8_t * capacity)
    return (block_size + sizeof(uint32_t) + sizeof(int8_t)) * capacity;
}

void ScryMemory_PoolInit(ScryPoolAllocator* pool, void* memory, size_t memory_size, size_t block_size, uint32_t capacity) {
    assert(pool != NULL);
    assert(memory != NULL);
    assert(memory_size >= ScryMemory_PoolGetRequiredSize(block_size, capacity));

    pool->data = memory;
    pool->next_free = (uint32_t*)((uint8_t*)memory + block_size * capacity);
    pool->states = (int8_t*)((uint8_t*)pool->next_free + sizeof(uint32_t) * capacity);
    pool->block_size = block_size;
    pool->capacity = capacity;
    pool->first_free = 0;
    pool->active_count = 0;

    for (uint32_t i = 0; i < capacity; ++i) {
        pool->next_free[i] = i + 1;
        pool->states[i] = 0;
    }
    pool->next_free[capacity - 1] = 0xFFFFFFFF;
}

uint32_t ScryMemory_PoolAllocate(ScryPoolAllocator* pool) {
    assert(pool != NULL);
    if (pool->first_free == 0xFFFFFFFF) return 0xFFFFFFFF;

    uint32_t index = pool->first_free;
    pool->first_free = pool->next_free[index];
    pool->states[index] = 1;
    pool->active_count++;
    return index;
}

void ScryMemory_PoolFree(ScryPoolAllocator* pool, uint32_t index) {
    assert(pool != NULL);
    assert(index < pool->capacity);
    if (pool->states[index] == 0) return;

    pool->next_free[index] = pool->first_free;
    pool->first_free = index;
    pool->states[index] = 0;
    pool->active_count--;
}

void ScryMemory_PoolReset(ScryPoolAllocator* pool) {
    assert(pool != NULL);
    pool->first_free = 0;
    pool->active_count = 0;
    for (uint32_t i = 0; i < pool->capacity; ++i) {
        pool->next_free[i] = i + 1;
        pool->states[i] = 0;
    }
    pool->next_free[pool->capacity - 1] = 0xFFFFFFFF;
}
