#include <engine/memory.h>
#include <engine/engine.h>
#include <cassert>
#include <new>
#include <cstdio>

namespace Engine {
namespace Memory {

/**
 * @brief Time to set up a new memory arena!
 * 
 * An arena is like a big bucket of memory that we can dip into whenever we need some.
 * 
 * @param arena The arena we're getting ready.
 * @param backing_memory The actual chunk of memory to use.
 * @param size How big that chunk of memory is.
 * 
 * @example
 * Arena my_arena;
 * void* memory = malloc(1024);
 * ArenaInit(&my_arena, memory, 1024);
 */
void ArenaInit(Arena* arena, void* backing_memory, size_t size) {
    assert(arena != nullptr);
    assert(backing_memory != nullptr);
    assert(size > 0);
    EngineLog("[Memory] Initializing a new arena...");
    EngineLog("[Memory] Arena setup in progress.");
    arena->backing_memory = static_cast<uint8_t*>(backing_memory);
    arena->total_size = size;
    arena->offset = 0;
}

/**
 * @brief Need a little bit of memory? Ask the arena!
 * 
 * This function carves out a slice of memory just for you, with the right alignment.
 * 
 * @param arena The arena to take memory from.
 * @param size How many bytes you need.
 * @param alignment How the memory should be aligned (e.g., 8 for 64-bit pointers).
 * @return A pointer to your new memory slice, or NULL if there's no room left.
 * 
 * @example
 * void* my_block = ArenaAllocate(&my_arena, 64, 8);
 */
void* ArenaAllocate(Arena* arena, size_t size, size_t alignment) {
    assert(arena != nullptr);
    assert(size > 0);
    assert(alignment > 0);
    EngineLog("[Memory] Allocating from arena...");
    if (!arena->backing_memory || size == 0) {
        EngineLog("[Memory] Arena allocation failed: invalid arena or size.");
        return nullptr;
    }

    const uintptr_t cur = reinterpret_cast<uintptr_t>(arena->backing_memory + arena->offset);
    const uintptr_t aligned = (cur + (alignment - 1)) & ~(alignment - 1);
    const size_t aligned_offset = aligned - reinterpret_cast<uintptr_t>(arena->backing_memory);

    if (aligned_offset + size > arena->total_size) {
        EngineLog("[Memory] Arena allocation failed: out of memory.");
        return nullptr;
    }
    arena->offset = aligned_offset + size;
    EngineLog("[Memory] Arena allocation successful.");
    return reinterpret_cast<void*>(aligned);
}

/**
 * @brief All done with that memory? Let's reset the arena.
 * 
 * This doesn't actually clear the memory, it just lets us start using it from the beginning again.
 * 
 * @param arena The arena to reset.
 * 
 * @example
 * ArenaReset(&my_arena);
 */
void ArenaReset(Arena* arena) {
    assert(arena != nullptr);
    assert(arena->backing_memory != nullptr);
    EngineLog("[Memory] Resetting arena offset...");
    arena->offset = 0;
    EngineLog("[Memory] Arena reset complete.");
}

/**
 * @brief Just a handy helper to make sure sizes are aligned correctly.
 * 
 * @param size The size you want to align.
 * @param alignment The alignment you need.
 * @return The smallest size that's at least 'size' and is a multiple of 'alignment'.
 * 
 * @example
 * size_t aligned = AlignSize(13, 8); // returns 16
 */
static size_t AlignSize(size_t size, size_t alignment) {
    assert(alignment > 0);
    assert((alignment & (alignment - 1)) == 0); // Must be a power of 2
    return (size + (alignment - 1)) & ~(alignment - 1);
}

/**
 * @brief Wondering how much memory a pool needs?
 * 
 * This calculates the total bytes required for a pool with a certain capacity and block size.
 * 
 * @param block_size The size of each block in the pool.
 * @param capacity How many blocks the pool should hold.
 * @return The total number of bytes needed.
 * 
 * @example
 * size_t bytes = PoolGetRequiredSize(sizeof(MyStruct), 100);
 */
size_t PoolGetRequiredSize(size_t block_size, uint32_t capacity) {
    assert(block_size > 0);
    assert(capacity > 0);
    const size_t data_size      = AlignSize(block_size * capacity, alignof(uint32_t));
    const size_t next_free_size = AlignSize(sizeof(uint32_t) * capacity, alignof(int8_t));
    const size_t states_size    = AlignSize(sizeof(int8_t) * capacity, sizeof(void*));
    return data_size + next_free_size + states_size;
}

/**
 * @brief Let's get a memory pool ready for action!
 * 
 * Pools are great for when you have lots of objects of the same size.
 * 
 * @param pool The pool allocator to initialize.
 * @param memory The memory to use for the pool.
 * @param memory_size The size of that memory.
 * @param block_size The size of each block.
 * @param capacity How many blocks to support.
 * 
 * @example
 * PoolAllocator my_pool;
 * PoolInit(&my_pool, memory, size, sizeof(MyStruct), 100);
 */
void PoolInit(PoolAllocator* pool, void* memory, size_t memory_size, size_t block_size, uint32_t capacity) {
    assert(pool && memory && memory_size > 0 && block_size > 0 && capacity > 0);
    assert(memory_size >= PoolGetRequiredSize(block_size, capacity));
    EngineLog("[Memory] Initializing pool allocator...");
    EngineLog("[Memory] Setting up pool blocks.");

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

/**
 * @brief Grab a block from the pool!
 * 
 * @param pool The pool to allocate from.
 * @return The index of the allocated block, or 0xFFFFFFFF if the pool is full.
 * 
 * @example
 * uint32_t index = PoolAllocate(&my_pool);
 */
uint32_t PoolAllocate(PoolAllocator* pool) {
    assert(pool != nullptr);
    assert(pool->data != nullptr);
    EngineLog("[Memory] Attempting pool allocation...");
    if (pool->first_free == 0xFFFFFFFF) {
        EngineLog("[Memory] Pool allocation failed: pool is full.");
        return 0xFFFFFFFF;
    }
    const uint32_t index = pool->first_free;
    pool->first_free = pool->next_free[index];
    pool->states[index] = 1;
    ++pool->active_count;
    EngineLog("[Memory] Pool allocation successful.");
    return index;
}

/**
 * @brief Done with a block? Put it back in the pool!
 * 
 * @param pool The pool to return it to.
 * @param index The index of the block to free.
 * 
 * @example
 * PoolFree(&my_pool, index);
 */
void PoolFree(PoolAllocator* pool, uint32_t index) {
    assert(pool != nullptr);
    assert(index < pool->capacity);
    EngineLog("[Memory] Freeing pool block...");
    if (index >= pool->capacity || pool->states[index] == 0) {
        EngineLog("[Memory] Pool free failed: invalid index or already free.");
        return;
    }
    pool->states[index] = 0;
    pool->next_free[index] = pool->first_free;
    pool->first_free = index;
    --pool->active_count;
    EngineLog("[Memory] Pool block freed.");
}

/**
 * @brief Time for a fresh start! Reset the whole pool.
 * 
 * This makes every block in the pool available again.
 * 
 * @param pool The pool to reset.
 * 
 * @example
 * PoolReset(&my_pool);
 */
void PoolReset(PoolAllocator* pool) {
    assert(pool != nullptr);
    assert(pool->states != nullptr);
    EngineLog("[Memory] Resetting pool allocator...");
    pool->first_free = 0;
    pool->active_count = 0;
    for (uint32_t i = 0; i < pool->capacity; ++i) {
        pool->states[i] = 0;
        pool->next_free[i] = (i < pool->capacity - 1) ? i + 1 : 0xFFFFFFFF;
    }
    EngineLog("[Memory] Pool reset complete.");
}

} // namespace Memory
} // namespace Engine
