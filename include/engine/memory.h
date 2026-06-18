#pragma once
#include <engine/engine.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ScryArena {
    uint8_t* backing_memory;
    size_t   total_size;
    size_t   offset;
} ScryArena;

/**
 * @brief Sets up a new memory arena.
 */
ENGINE_API void ScryMemory_ArenaInit(ScryArena* arena, void* backing_memory, size_t size);

/**
 * @brief Allocates a chunk of memory from the arena.
 */
ENGINE_API void* ScryMemory_ArenaAllocate(ScryArena* arena, size_t size, size_t alignment);

/**
 * @brief Resets the arena to its initial state.
 */
ENGINE_API void ScryMemory_ArenaReset(ScryArena* arena);

typedef struct ScryPoolAllocator {
    void*     data;
    uint32_t* next_free;
    int8_t*   states;
    size_t    block_size;
    uint32_t  capacity;
    uint32_t  first_free;
    uint32_t  active_count;
} ScryPoolAllocator;

/**
 * @brief Calculates memory required for a pool allocator.
 */
ENGINE_API size_t ScryMemory_PoolGetRequiredSize(size_t block_size, uint32_t capacity);

/**
 * @brief Initializes a pool allocator.
 */
ENGINE_API void ScryMemory_PoolInit(ScryPoolAllocator* pool, void* memory, size_t memory_size, size_t block_size, uint32_t capacity);

/**
 * @brief Allocates a block from the pool.
 */
ENGINE_API uint32_t ScryMemory_PoolAllocate(ScryPoolAllocator* pool);

/**
 * @brief Returns a block to the pool.
 */
ENGINE_API void ScryMemory_PoolFree(ScryPoolAllocator* pool, uint32_t index);

/**
 * @brief Resets the entire pool.
 */
ENGINE_API void ScryMemory_PoolReset(ScryPoolAllocator* pool);

/**
 * @brief Gets a pointer to a block at a specific index.
 */
static inline void* ScryMemory_PoolGet(ScryPoolAllocator* pool, uint32_t index) {
    if (index >= pool->capacity || pool->states[index] == 0) return NULL;
    return (uint8_t*)pool->data + (index * pool->block_size);
}

/* ── Frame arena ─────────────────────────────────────────────────────────────
 * 64 MB linear allocator reset in Phase_Cleanup each frame. Use Arena_Alloc
 * for any per-frame scratch memory that must not outlive the frame.
 */
ENGINE_API extern ScryArena g_FrameArena;

ENGINE_API void* Arena_Alloc(size_t size);  /* 16-byte aligned; returns NULL on overflow */
ENGINE_API void  Arena_Reset(void);         /* Resets offset to 0; called by Phase_Cleanup */

#ifdef __cplusplus
}
#endif
