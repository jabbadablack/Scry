#include <doctest/doctest.h>
#include <memory/chained_arena.hpp>
#include <memory/array.hpp>
#include <math/algebra.hpp>

struct MockDestructible {
    static int destruction_count;
    int value = 0;

    MockDestructible() = default;
    explicit MockDestructible(int val) : value(val) {}
    ~MockDestructible() {
        destruction_count++;
    }
};

int MockDestructible::destruction_count = 0;

TEST_CASE("ArenaArray Core Verification") {
    engine::ChainedArena arena(2048);

    SUBCASE("Successful construction, population, and bounds checking") {
        engine::ArenaArray<int> arr(arena, 5);
        REQUIRE(arr.size() == 0);
        REQUIRE(arr.capacity() == 5);

        arr.PushBack(10);
        arr.PushBack(20);
        arr.PushBack(30);

        REQUIRE(arr.size() == 3);
        REQUIRE(arr[0] == 10);
        REQUIRE(arr[1] == 20);
        REQUIRE(arr[2] == 30);

        int sum = 0;
        for (int val : arr) {
            sum += val;
        }
        REQUIRE(sum == 60);
    }

    SUBCASE("Verify object destructors are called when ArenaArray goes out of scope") {
        MockDestructible::destruction_count = 0;

        MockDestructible m1(1);
        MockDestructible m2(2);

        {
            engine::ArenaArray<MockDestructible> arr(arena, 3);
            arr.PushBack(m1);
            arr.PushBack(m2);
            MockDestructible::destruction_count = 0;
        }

        REQUIRE(MockDestructible::destruction_count == 2);
        MockDestructible::destruction_count = 0;
    }

    SUBCASE("ArenaArray Clear and capacity reuse functionality") {
        MockDestructible::destruction_count = 0;
        MockDestructible m1(1);
        MockDestructible m2(2);

        {
            engine::ArenaArray<MockDestructible> arr(arena, 4);
            arr.PushBack(m1);
            arr.PushBack(m2);

            MockDestructible::destruction_count = 0;
            arr.Clear();

            REQUIRE(MockDestructible::destruction_count == 2);
            REQUIRE(arr.size() == 0);
            REQUIRE(arr.capacity() == 4);

            arr.PushBack(m1);
            REQUIRE(arr.size() == 1);
            REQUIRE(arr[0].value == 1);

            MockDestructible::destruction_count = 0;
        }
        MockDestructible::destruction_count = 0;
    }

    SUBCASE("Math integration and SIMD alignment verification") {
        engine::ArenaArray<engine::math::Vector4> vec_arr(arena, 4);

        REQUIRE(reinterpret_cast<uintptr_t>(vec_arr.data()) % alignof(engine::math::Vector4) == 0);

        vec_arr.PushBack(engine::math::Vector4(1.0f, 2.0f, 3.0f, 4.0f));
        vec_arr.PushBack(engine::math::Vector4(5.0f, 6.0f, 7.0f, 8.0f));

        REQUIRE(vec_arr.size() == 2);
        REQUIRE(vec_arr[0].x() == 1.0f);
        REQUIRE(vec_arr[0].y() == 2.0f);
        REQUIRE(vec_arr[0].z() == 3.0f);
        REQUIRE(vec_arr[0].w() == 4.0f);

        REQUIRE(vec_arr[1].x() == 5.0f);
        REQUIRE(vec_arr[1].y() == 6.0f);
        REQUIRE(vec_arr[1].z() == 7.0f);
        REQUIRE(vec_arr[1].w() == 8.0f);
    }
}
