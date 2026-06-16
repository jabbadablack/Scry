#pragma once
#include <engine/engine.h>
#include <cstddef>
#include <cstdint>
#include <cassert>
#include <cstdio>

namespace Engine {
namespace Memory {

struct ENGINE_API Arena {
    uint8_t* backing_memory = nullptr;
    size_t   total_size     = 0;
    size_t   offset         = 0;
};

/**
 * @brief Sets up a new memory arena.
 *
 * Hello! This function prepares an arena with a block of memory you provide.
 * It's a super fast way to handle temporary allocations!
 *
 * @param arena The arena structure to initialize.
 * @param backing_memory Pointer to the raw memory buffer.
 * @param size The total size of the buffer in bytes.
 *
 * @example
 * void* buffer = malloc(1024);
 * Engine::Memory::Arena arena;
 * Engine::Memory::ArenaInit(&arena, buffer, 1024);
 */
ENGINE_API void  ArenaInit(Arena* arena, void* backing_memory, size_t size);

/**
 * @brief Allocates a chunk of memory from the arena.
 *
 * Hi there! Need some memory? This function gives you a piece from
 * the arena. It's very efficient because it just moves a pointer!
 *
 * @param arena The arena to allocate from.
 * @param size How many bytes you need.
 * @param alignment The byte alignment for the allocation.
 * @return A pointer to the allocated memory, or NULL if there's no room.
 *
 * @example
 * void* data = Engine::Memory::ArenaAllocate(&arena, 256);
 */
ENGINE_API void* ArenaAllocate(Arena* arena, size_t size, size_t alignment = sizeof(void*));

/**
 * @brief Resets the arena to its initial state.
 *
 * Greetings! This function clears all allocations in the arena at once.
 * It's like wiping a chalkboard clean!
 *
 * @param arena The arena to reset.
 *
 * @example
 * Engine::Memory::ArenaReset(&arena);
 */
ENGINE_API void  ArenaReset(Arena* arena);

/**
 * @brief Returns how much memory has been used in the arena.
 *
 * Hello! Curious about your memory usage? This function tells you
 * exactly how many bytes have been allocated so far.
 *
 * @param arena The arena to check.
 * @return The number of bytes currently in use.
 *
 * @example
 * size_t used = Engine::Memory::ArenaGetUsedMemory(&arena);
 */
inline size_t ArenaGetUsedMemory(const Arena* arena) {
    assert(arena != nullptr);
    assert(arena->backing_memory != nullptr);
    static bool logged_once = false;
    if (!logged_once) {
        EngineLog("ArenaGetUsedMemory called");
        EngineLog("Checking arena offset");
        logged_once = true;
    }
    return arena->offset;
}

/**
 * @brief Returns the total capacity of the arena.
 *
 * Hi! This function lets you know the maximum size of your arena.
 * It's good to know your limits!
 *
 * @param arena The arena to check.
 * @return The total size of the arena in bytes.
 *
 * @example
 * size_t total = Engine::Memory::ArenaGetTotalSize(&arena);
 */
inline size_t ArenaGetTotalSize(const Arena* arena) {
    assert(arena != nullptr);
    assert(arena->total_size > 0);
    static bool logged_once = false;
    if (!logged_once) {
        EngineLog("ArenaGetTotalSize called");
        EngineLog("Fetching total capacity");
        logged_once = true;
    }
    return arena->total_size;
}

/**
 * @brief Calculates how much free space remains in the arena.
 *
 * Greetings! If you're worried about running out of space, this function
 * tells you exactly how many more bytes you can allocate.
 *
 * @param arena The arena to check.
 * @return The number of remaining bytes.
 *
 * @example
 * size_t remaining = Engine::Memory::ArenaGetRemainingMemory(&arena);
 */
inline size_t ArenaGetRemainingMemory(const Arena* arena) {
    assert(arena != nullptr);
    assert(arena->total_size >= arena->offset);
    static bool logged_once = false;
    if (!logged_once) {
        EngineLog("ArenaGetRemainingMemory called");
        EngineLog("Calculating remaining space");
        logged_once = true;
    }
    return arena->total_size - arena->offset;
}

struct ENGINE_API PoolAllocator {
    void*     data         = nullptr;
    uint32_t* next_free    = nullptr;
    int8_t*   states       = nullptr;
    size_t    block_size   = 0;
    uint32_t  capacity     = 0;
    uint32_t  first_free   = 0xFFFFFFFF;
    uint32_t  active_count = 0;
};

/**
 * @brief Calculates the memory required for a pool allocator.
 *
 * Hello! Before you set up a pool, use this to find out how much
 * memory it will need for all its internal structures.
 *
 * @param block_size The size of each individual block in the pool.
 * @param capacity The maximum number of blocks the pool can hold.
 * @return The total size in bytes required for the pool.
 *
 * @example
 * size_t needed = Engine::Memory::PoolGetRequiredSize(64, 100);
 */
ENGINE_API size_t   PoolGetRequiredSize(size_t block_size, uint32_t capacity);

/**
 * @brief Initializes a pool allocator.
 *
 * Hi there! This function sets up a fixed-size block allocator.
 * It's perfect for when you have many objects of the same size!
 *
 * @param pool The pool structure to initialize.
 * @param memory Pointer to the memory buffer to use.
 * @param memory_size The size of the provided memory buffer.
 * @param block_size The size of each block.
 * @param capacity The maximum number of blocks.
 *
 * @example
 * Engine::Memory::PoolAllocator my_pool;
 * Engine::Memory::PoolInit(&my_pool, buffer, size, 64, 100);
 */
ENGINE_API void     PoolInit(PoolAllocator* pool, void* memory, size_t memory_size, size_t block_size, uint32_t capacity);

/**
 * @brief Allocates a block from the pool.
 *
 * Greetings! This function gives you the index of a free block in the pool.
 * It's super fast and great for avoiding memory fragmentation!
 *
 * @param pool The pool to allocate from.
 * @return The index of the allocated block, or 0xFFFFFFFF if the pool is full.
 *
 * @example
 * uint32_t index = Engine::Memory::PoolAllocate(&my_pool);
 */
ENGINE_API uint32_t PoolAllocate(PoolAllocator* pool);

/**
 * @brief Returns a block to the pool.
 *
 * Hi! Done with a block? Give it back to the pool so someone else
 * can use it. We like to keep things tidy!
 *
 * @param pool The pool to return the block to.
 * @param index The index of the block to free.
 *
 * @example
 * Engine::Memory::PoolFree(&my_pool, index);
 */
ENGINE_API void     PoolFree(PoolAllocator* pool, uint32_t index);

/**
 * @brief Resets the entire pool.
 *
 * Hello! This function frees all blocks in the pool instantly.
 * Very handy for clearing out a whole bunch of objects!
 *
 * @param pool The pool to reset.
 *
 * @example
 * Engine::Memory::PoolReset(&my_pool);
 */
ENGINE_API void     PoolReset(PoolAllocator* pool);

/**
 * @brief Gets a pointer to a block at a specific index.
 *
 * Greetings! This function translates a pool index into a real memory
 * pointer so you can actually use the data.
 *
 * @param pool The pool containing the block.
 * @param index The index of the block.
 * @return A pointer to the block's data, or NULL if the index is invalid.
 *
 * @example
 * void* data = Engine::Memory::PoolGet(&my_pool, index);
 */
inline void* PoolGet(PoolAllocator* pool, uint32_t index) {
    assert(pool != nullptr);
    assert(pool->data != nullptr);
    static bool logged_once = false;
    if (!logged_once) {
        EngineLog("PoolGet called");
        char msg[64];
        std::snprintf(msg, sizeof(msg), "Accessing pool index %u", index);
        EngineLog(msg);
        logged_once = true;
    }
    if (index >= pool->capacity || pool->states[index] == 0) return nullptr;
    return static_cast<uint8_t*>(pool->data) + (index * pool->block_size);
}

/**
 * @brief Gets a read-only pointer to a block.
 *
 * Hi! Like PoolGet, but for when you only need to look at the data
 * and promise not to change anything.
 *
 * @param pool The pool containing the block.
 * @param index The index of the block.
 * @return A constant pointer to the block's data, or NULL if invalid.
 *
 * @example
 * const void* data = Engine::Memory::PoolGetConst(&my_pool, index);
 */
inline const void* PoolGetConst(const PoolAllocator* pool, uint32_t index) {
    assert(pool != nullptr);
    assert(pool->states != nullptr);
    static bool logged_once = false;
    if (!logged_once) {
        EngineLog("PoolGetConst called");
        EngineLog("Performing bounds check");
        logged_once = true;
    }
    if (index >= pool->capacity || pool->states[index] == 0) return nullptr;
    return static_cast<const uint8_t*>(pool->data) + (index * pool->block_size);
}

/**
 * @brief Returns the maximum number of blocks the pool can hold.
 *
 * Hello! Use this to check how many blocks your pool was built for.
 *
 * @param pool The pool to check.
 * @return The total capacity of the pool.
 *
 * @example
 * size_t cap = Engine::Memory::PoolGetCapacity(&my_pool);
 */
inline size_t PoolGetCapacity(const PoolAllocator* pool) {
    assert(pool != nullptr);
    assert(pool->capacity > 0);
    static bool logged_once = false;
    if (!logged_once) {
        EngineLog("PoolGetCapacity called");
        EngineLog("Returning pool capacity");
        logged_once = true;
    }
    return pool->capacity;
}

/**
 * @brief Returns the number of blocks currently in use.
 *
 * Greetings! This function tells you how many blocks are currently
 * checked out of the pool.
 *
 * @param pool The pool to check.
 * @return The number of active blocks.
 *
 * @example
 * size_t count = Engine::Memory::PoolGetActiveCount(&my_pool);
 */
inline size_t PoolGetActiveCount(const PoolAllocator* pool) {
    assert(pool != nullptr);
    assert(pool->active_count <= pool->capacity);
    static bool logged_once = false;
    if (!logged_once) {
        EngineLog("PoolGetActiveCount called");
        EngineLog("Fetching active count");
        logged_once = true;
    }
    return pool->active_count;
}

} // namespace Memory
} // namespace Engine
