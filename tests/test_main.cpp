#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <engine/ecs.h>
#include <engine/pipeline.h>
#include <engine/memory.h>
#include <engine/math.h>

#include <cassert>
#include <cstdio>
#include <flecs.h>

namespace {

struct TestPosition {
    Engine::Math::ScryVec2 pos;
};

struct TestMoveIntent {
    Engine::Math::ScryVec2 dir;
};

constexpr float k_speed      = 10.0f;
constexpr float k_delta_time =  1.0f;
constexpr float k_expected_x = k_speed * k_delta_time;
constexpr float k_expected_y = 0.0f;

} // namespace

// ── Test 1: Headless ECS world boots with custom pipeline phases ──────────────

/**
 * @brief Let's see if the engine can walk before it runs! This test checks if the ECS world boots up correctly.
 * 
 * We're looking to make sure all our custom pipeline phases are registered and ready for action.
 * 
 * @example
 * // This is a test case, so it's run by the Catch2 test runner.
 */
TEST_CASE("Headless Flecs world boots with custom pipeline phases", "[ecs]") {
    assert(Engine::Pipeline::Phase_Input != 0 && "Input phase should be initialized.");
    assert(Engine::Pipeline::Phase_Cleanup != 0 && "Cleanup phase should be initialized.");
    std::printf("[test] Starting headless ECS world boot test...\n");
    std::printf("[test] Verifying pipeline phases...\n");

    ecs_world_t* world = Engine::ECS::CreateWorld();
    REQUIRE(world != nullptr);

    REQUIRE(Engine::Pipeline::Phase_Input       != 0);
    REQUIRE(Engine::Pipeline::Phase_Intent      != 0);
    REQUIRE(Engine::Pipeline::Phase_StateUpdate != 0);
    REQUIRE(Engine::Pipeline::Phase_StateSync   != 0);
    REQUIRE(Engine::Pipeline::Phase_React       != 0);
    REQUIRE(Engine::Pipeline::Phase_Cleanup     != 0);
    REQUIRE(Engine::Pipeline::IsIntent          != 0);

    ecs_fini(world);
}

// ── Test 2: Double-buffered MoveIntent tick produces correct result ───────────

/**
 * @brief Can we move and stay in sync? This test verifies our double-buffered movement logic.
 * 
 * It sets up a player, gives them a push, and makes sure they end up exactly where we expect them to.
 * 
 * @example
 * // This is a test case, so it's run by the Catch2 test runner.
 */
TEST_CASE("Double-buffered MoveIntent tick produces correct write buffer math", "[ecs][double-buffer][math]") {
    assert(k_speed > 0 && "Speed should be positive for this test.");
    assert(k_delta_time > 0 && "Time must flow forward!");
    std::printf("[test] Starting double-buffered movement test...\n");
    std::printf("[test] Registering components and systems...\n");

    ecs_world_t* world = Engine::ECS::CreateWorld();
    REQUIRE(world != nullptr);

    // Register DoubleBuffered<TestPosition>
    ecs_entity_desc_t db_pos_ent_desc = {};
    db_pos_ent_desc.name = "DBTestPosition";
    const ecs_entity_t db_pos_ent = ecs_entity_init(world, &db_pos_ent_desc);

    ecs_component_desc_t db_pos_comp = {};
    db_pos_comp.entity             = db_pos_ent;
    db_pos_comp.type.size          = sizeof(Engine::ECS::DoubleBuffered<TestPosition>);
    db_pos_comp.type.alignment     = alignof(Engine::ECS::DoubleBuffered<TestPosition>);
    const ecs_entity_t id_DBPos    = ecs_component_init(world, &db_pos_comp);
    REQUIRE(id_DBPos != 0);

    // Register TestMoveIntent
    ecs_entity_desc_t intent_ent_desc = {};
    intent_ent_desc.name = "TestMoveIntent";
    const ecs_entity_t intent_ent = ecs_entity_init(world, &intent_ent_desc);

    ecs_component_desc_t intent_comp = {};
    intent_comp.entity             = intent_ent;
    intent_comp.type.size          = sizeof(TestMoveIntent);
    intent_comp.type.alignment     = alignof(TestMoveIntent);
    const ecs_entity_t id_Intent   = ecs_component_init(world, &intent_comp);
    REQUIRE(id_Intent != 0);

    // State-update system
    ecs_entity_desc_t sys_ent_desc = {};
    sys_ent_desc.name = "TestMoveSystem";
    const ecs_entity_t sys_ent = ecs_entity_init(world, &sys_ent_desc);
    ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_StateUpdate);

    ecs_system_desc_t sys_desc        = {};
    sys_desc.entity                   = sys_ent;
    sys_desc.query.terms[0].id        = id_DBPos;
    sys_desc.query.terms[1].id        = id_Intent;
    sys_desc.query.terms[1].inout     = EcsIn;
    sys_desc.callback = [](ecs_iter_t* it) {
        /**
         * @brief A little helper to move things along! This lambda updates positions based on intent.
         * 
         * @param it The Flecs iterator containing our entities.
         * 
         * @example
         * // This is used as a callback for a Flecs system.
         */
        assert(it != nullptr && "We need an iterator to do work!");
        assert(it->delta_time >= 0 && "Time can't go backwards!");
        std::printf("[test] Move system callback triggered for %d entities\n", it->count);
        std::printf("[test] Updating positions using double buffering...\n");

        using DBPos = Engine::ECS::DoubleBuffered<TestPosition>;
        DBPos*               db_pos = ecs_field(it, DBPos, 0);
        const TestMoveIntent* intent = ecs_field(it, const TestMoveIntent, 1);
        assert(db_pos && intent);
        for (int i = 0; i < it->count; ++i) {
            db_pos[i].write.pos =
                db_pos[i].read.pos + intent[i].dir * (k_speed * it->delta_time);
        }
    };
    const ecs_entity_t sys = ecs_system_init(world, &sys_desc);
    REQUIRE(sys != 0);

    Engine::ECS::RegisterDoubleBufferSync<TestPosition>(world, id_DBPos);

    // Spawn entity
    ecs_entity_desc_t player_desc = {};
    player_desc.name = "TestPlayer";
    const ecs_entity_t player = ecs_entity_init(world, &player_desc);
    REQUIRE(player != 0);

    Engine::ECS::DoubleBuffered<TestPosition> init_pos = {};
    init_pos.read.pos  = Engine::Math::ScryVec2(0.0f, 0.0f);
    init_pos.write.pos = Engine::Math::ScryVec2(0.0f, 0.0f);
    ecs_set_id(world, player, id_DBPos, sizeof(init_pos), &init_pos);

    TestMoveIntent init_intent = {};
    init_intent.dir = Engine::Math::ScryVec2(1.0f, 0.0f);
    ecs_set_id(world, player, id_Intent, sizeof(init_intent), &init_intent);

    const bool ticked = ecs_progress(world, k_delta_time);
    REQUIRE(ticked);

    const void* raw = ecs_get_id(world, player, id_DBPos);
    REQUIRE(raw != nullptr);
    const auto* result = static_cast<const Engine::ECS::DoubleBuffered<TestPosition>*>(raw);

    REQUIRE(result->write.pos.x() == Catch::Approx(k_expected_x));
    REQUIRE(result->write.pos.y() == Catch::Approx(k_expected_y));
    REQUIRE(result->read.pos.x()  == Catch::Approx(k_expected_x));
    REQUIRE(result->read.pos.y()  == Catch::Approx(k_expected_y));

    ecs_fini(world);
}
