#include <doctest/doctest.h>
#include <memory/chained_arena.hpp>
#include <memory/pool_allocator.hpp>

TEST_CASE("ChainedArena Allocator Core Functionality") {
    engine::ChainedArena arena(1024);

    SUBCASE("Successful allocations and O(1) clear capacity reuse") {
        std::byte* ptr1 = arena.Allocate(16, 8);
        REQUIRE(ptr1 != nullptr);
        REQUIRE(reinterpret_cast<uintptr_t>(ptr1) % 8 == 0);

        std::byte* ptr2 = arena.Allocate(32, 16);
        REQUIRE(ptr2 != nullptr);
        REQUIRE(reinterpret_cast<uintptr_t>(ptr2) % 16 == 0);
        REQUIRE(ptr2 > ptr1);

        arena.Clear();
        std::byte* ptr3 = arena.Allocate(16, 8);
        REQUIRE(ptr3 == ptr1);
    }
}

TEST_CASE("PoolAllocator Core Functionality") {
    engine::PoolAllocator pool(32, 4);

    SUBCASE("Allocation and Free reuse") {
        void* p1 = pool.Allocate();
        REQUIRE(p1 != nullptr);

        void* p2 = pool.Allocate();
        REQUIRE(p2 != nullptr);
        REQUIRE(p2 != p1);

        void* p3 = pool.Allocate();
        void* p4 = pool.Allocate();
        REQUIRE(p3 != nullptr);
        REQUIRE(p4 != nullptr);

        pool.Free(p2);
        void* p5 = pool.Allocate();
        REQUIRE(p5 == p2);
    }
}
