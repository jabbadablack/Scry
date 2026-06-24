#include <doctest/doctest.h>
#include <memory/chained_arena.hpp>

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
