#include <catch2/catch_test_macros.hpp>
#include <engine/memory.h>

TEST_CASE("Arena Allocator basic functionality", "[memory]") {
    uint8_t buffer[1024];
    ScryArena arena;
    ScryMemory_ArenaInit(&arena, buffer, 1024);

    SECTION("Allocation is aligned and within bounds") {
        void* p1 = ScryMemory_ArenaAllocate(&arena, 10, 8);
        REQUIRE(p1 != nullptr);
        REQUIRE(((uintptr_t)p1 % 8) == 0);
        
        void* p2 = ScryMemory_ArenaAllocate(&arena, 20, 16);
        REQUIRE(p2 != nullptr);
        REQUIRE(((uintptr_t)p2 % 16) == 0);
        REQUIRE((uint8_t*)p2 >= (uint8_t*)p1 + 10);
    }

    SECTION("Arena exhaustion returns NULL") {
        void* p = ScryMemory_ArenaAllocate(&arena, 1000, 1);
        REQUIRE(p != nullptr);
        void* p2 = ScryMemory_ArenaAllocate(&arena, 50, 1);
        REQUIRE(p2 == nullptr);
    }

    SECTION("Arena reset works") {
        ScryMemory_ArenaAllocate(&arena, 500, 1);
        ScryMemory_ArenaReset(&arena);
        void* p = ScryMemory_ArenaAllocate(&arena, 1024, 1);
        REQUIRE(p != nullptr);
    }
}

TEST_CASE("Pool Allocator basic functionality", "[memory]") {
    const size_t block_size = 64;
    const uint32_t capacity = 10;
    size_t needed = ScryMemory_PoolGetRequiredSize(block_size, capacity);
    void* buffer = malloc(needed);
    
    ScryPoolAllocator pool;
    ScryMemory_PoolInit(&pool, buffer, needed, block_size, capacity);

    SECTION("Allocating and freeing works") {
        uint32_t i1 = ScryMemory_PoolAllocate(&pool);
        REQUIRE(i1 != 0xFFFFFFFF);
        
        void* p1 = ScryMemory_PoolGet(&pool, i1);
        REQUIRE(p1 != nullptr);

        ScryMemory_PoolFree(&pool, i1);
        uint32_t i2 = ScryMemory_PoolAllocate(&pool);
        REQUIRE(i2 == i1); // Should reuse the block
    }

    SECTION("Pool exhaustion") {
        for (uint32_t i = 0; i < capacity; ++i) {
            REQUIRE(ScryMemory_PoolAllocate(&pool) != 0xFFFFFFFF);
        }
        REQUIRE(ScryMemory_PoolAllocate(&pool) == 0xFFFFFFFF);
    }

    free(buffer);
}
