#include <doctest/doctest.h>
#include <ecs/registry.hpp>
#include <memory/tracked_heap.hpp>
#include <debug/logger.hpp>

// Sample components for test cases
struct Position {
    float x = 0.0f;
    float y = 0.0f;
};

struct Velocity {
    float dx = 0.0f;
    float dy = 0.0f;
};

struct TagActive {};

TEST_CASE("ECS Registry Operations and Tracked Memory") {
    engine::ecs::Registry registry;

    SUBCASE("Entity creation and destruction") {
        entt::entity e1 = registry.CreateEntity();
        entt::entity e2 = registry.CreateEntity();

        REQUIRE((e1 != entt::null));
        REQUIRE((e2 != entt::null));
        REQUIRE((e1 != e2));

        // Verify entity validity
        REQUIRE(registry.GetRawRegistry().valid(e1));
        REQUIRE(registry.GetRawRegistry().valid(e2));

        registry.DestroyEntity(e1);
        REQUIRE(!registry.GetRawRegistry().valid(e1));
        REQUIRE(registry.GetRawRegistry().valid(e2));
    }

    SUBCASE("Component addition, querying via View(), and removal") {
        entt::entity e1 = registry.CreateEntity();
        entt::entity e2 = registry.CreateEntity();

        // Add components with parameters
        registry.AddComponent<Position>(e1, 10.0f, 20.0f);
        registry.AddComponent<Velocity>(e1, 1.0f, 2.0f);
        registry.AddComponent<Position>(e2, 30.0f, 40.0f);

        // Verify component presence
        REQUIRE(registry.HasComponent<Position>(e1));
        REQUIRE(registry.HasComponent<Velocity>(e1));
        REQUIRE(registry.HasComponent<Position>(e2));
        REQUIRE(!registry.HasComponent<Velocity>(e2));

        // Verify values
        Position& pos = registry.GetComponent<Position>(e1);
        REQUIRE(pos.x == 10.0f);
        REQUIRE(pos.y == 20.0f);

        // Query view matching Position and Velocity
        auto view = registry.View<Position, Velocity>();
        int count = 0;
        for (auto entity : view) {
            count++;
            Position& p = view.get<Position>(entity);
            Velocity& v = view.get<Velocity>(entity);
            REQUIRE(entity == e1);
            REQUIRE(p.x == 10.0f);
            REQUIRE(v.dx == 1.0f);
        }
        REQUIRE(count == 1);

        // Remove component and verify
        registry.RemoveComponent<Velocity>(e1);
        REQUIRE(!registry.HasComponent<Velocity>(e1));
    }

    SUBCASE("Tracked Memory Verification") {
        // EnTT dynamically allocates pools and sparse/dense sets to store components.
        // Adding components to the registry should trigger allocations inside EcsAllocator,
        // which routes to TrackedHeap.
        
        // Let's check memory state before component allocation
        size_t initial_usage = engine::TrackedHeap::GetCurrentUsage();
        
        entt::entity e1 = registry.CreateEntity();
        
        // Adding a component requires EnTT to initialize a storage pool (which allocates memory dynamically).
        // Since we overridden the registry to use EcsAllocator, it will allocate via TrackedHeap.
        registry.AddComponent<Position>(e1, 100.0f, 200.0f);
        
        // Assert that the TrackedHeap tracked some allocations
        size_t post_usage = engine::TrackedHeap::GetCurrentUsage();
        
        ENGINE_LOG_INFO("[ENGINE TEST] TrackedHeap Usage - Initial: " + std::to_string(initial_usage) +
                       " bytes, Post-component: " + std::to_string(post_usage) + " bytes");
                  
        REQUIRE(post_usage > initial_usage);
        REQUIRE(engine::TrackedHeap::GetTotalAllocated() > 0);
        REQUIRE(engine::TrackedHeap::GetPeakUsage() > 0);
    }

    SUBCASE("Reactive observer test") {
        auto observer = registry.ObserveCreation<TagActive>();
        REQUIRE(observer.empty());

        entt::entity e1 = registry.CreateEntity();
        entt::entity e2 = registry.CreateEntity();

        // Add component to e1 - triggers observer
        registry.AddComponent<TagActive>(e1);
        REQUIRE(!observer.empty());
        REQUIRE(observer.size() == 1);
        REQUIRE(*observer.data() == e1);

        // Clear observer accumulated list
        observer.clear();
        REQUIRE(observer.empty());

        // Add component to e2 - triggers observer again
        registry.AddComponent<TagActive>(e2);
        REQUIRE(!observer.empty());
        REQUIRE(observer.size() == 1);
        REQUIRE(*observer.data() == e2);
        
        observer.disconnect();
    }
}
