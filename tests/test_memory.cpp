#include <catch2/catch_test_macros.hpp>
#include <scry/scry_memory.hpp>

TEST_CASE("Arena Allocator basic allocation", "[memory]") {
    const size_t arena_size = 1024;
    void* backing = ::operator new(arena_size);
    REQUIRE(backing != nullptr);

    Scry::Memory::Arena arena;
    arena.Init(backing, arena_size);
    REQUIRE(arena.GetTotalSize() == arena_size);
    REQUIRE(arena.GetUsedMemory() == 0);

    void* p1 = arena.Allocate(128, 8);
    REQUIRE(p1 != nullptr);
    REQUIRE(arena.GetUsedMemory() == 128);

    ::operator delete(backing);
}
