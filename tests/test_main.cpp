#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <engine/ecs.hpp>
#include <engine/pipeline.hpp>
#include <engine/memory.hpp>
#include <engine/math.hpp>

#include <libassert/assert.hpp>
#include <flecs.h>

// ---------------------------------------------------------------------------
// Local component types — defined here so the test owns its own ECS schema
// and has zero dependency on game-layer types defined in examples/.
// ---------------------------------------------------------------------------
namespace {

struct TestPosition {
    Engine::Math::ScryVec2 pos;
};

struct TestMoveIntent {
    Engine::Math::ScryVec2 dir;
};

constexpr float k_speed      = 10.0f;
constexpr float k_delta_time =  1.0f;
// Expected write.pos.x after one tick: 0 + 1*(10*1) = 10
constexpr float k_expected_x = k_speed * k_delta_time;
constexpr float k_expected_y = 0.0f;

} // namespace

// ---------------------------------------------------------------------------
// Test 1 — mimalloc is active and routes through the DLL's private heap
// ---------------------------------------------------------------------------
TEST_CASE("mimalloc routes DLL allocations through its own heap", "[memory]") {
    void* ptr = Engine::Memory::AllocInDll(256);
    REQUIRE(ptr != nullptr);
    REQUIRE(Engine::Memory::IsUsingMimalloc(ptr));
    Engine::Memory::FreeInDll(ptr);
}

// ---------------------------------------------------------------------------
// Test 2 — headless ECS world boots and exposes all six custom pipeline phases
// ---------------------------------------------------------------------------
TEST_CASE("Headless Flecs world boots with custom pipeline phases", "[ecs]") {
    ecs_world_t* world = Engine::ECS::CreateWorld();
    REQUIRE(world != nullptr);

    REQUIRE(Engine::Pipeline::Phase_Input       != 0);
    REQUIRE(Engine::Pipeline::Phase_Intent      != 0);
    REQUIRE(Engine::Pipeline::Phase_StateUpdate != 0);
    REQUIRE(Engine::Pipeline::Phase_StateSync   != 0);
    REQUIRE(Engine::Pipeline::Phase_React       != 0);
    REQUIRE(Engine::Pipeline::Phase_Cleanup     != 0);
    REQUIRE(Engine::Pipeline::IsIntent              != 0);

    ecs_fini(world);
}

// ---------------------------------------------------------------------------
// Test 3 — double-buffered MoveIntent tick produces the correct Write state
//
// Pipeline execution order for one ecs_progress(world, 1.0f):
//   Phase_Input       — nothing registered
//   Phase_Intent      — nothing registered
//   Phase_StateUpdate — TestMoveSystem: write.pos = read.pos + dir*(speed*dt)
//   Phase_StateSync   — SyncDoubleBuffer: read.pos = write.pos
//   Phase_React       — nothing registered
//   Phase_Cleanup     — IntentCleanupSystem (no IsIntent comps registered here)
//
// Initial state: read=(0,0), write=(0,0), dir=(1,0)
// After tick:    write=(10,0), read=(10,0)
// ---------------------------------------------------------------------------
TEST_CASE("Double-buffered MoveIntent tick produces correct write buffer math", "[ecs][double-buffer][math]") {

    // --- Boot a headless world -------------------------------------------
    ecs_world_t* world = Engine::ECS::CreateWorld();
    REQUIRE(world != nullptr);

    // --- Register DoubleBuffered<TestPosition> component -----------------
    ecs_entity_desc_t db_pos_ent_desc = {};
    db_pos_ent_desc.name = "DBTestPosition";
    const ecs_entity_t db_pos_ent = ecs_entity_init(world, &db_pos_ent_desc);

    ecs_component_desc_t db_pos_comp = {};
    db_pos_comp.entity             = db_pos_ent;
    db_pos_comp.type.size          = sizeof(Engine::ECS::DoubleBuffered<TestPosition>);
    db_pos_comp.type.alignment     = alignof(Engine::ECS::DoubleBuffered<TestPosition>);
    const ecs_entity_t id_DBPos    = ecs_component_init(world, &db_pos_comp);
    REQUIRE(id_DBPos != 0);

    // --- Register TestMoveIntent component --------------------------------
    ecs_entity_desc_t intent_ent_desc = {};
    intent_ent_desc.name = "TestMoveIntent";
    const ecs_entity_t intent_ent = ecs_entity_init(world, &intent_ent_desc);

    ecs_component_desc_t intent_comp = {};
    intent_comp.entity             = intent_ent;
    intent_comp.type.size          = sizeof(TestMoveIntent);
    intent_comp.type.alignment     = alignof(TestMoveIntent);
    const ecs_entity_t id_Intent   = ecs_component_init(world, &intent_comp);
    REQUIRE(id_Intent != 0);

    // --- Register state-update system on Phase_StateUpdate -----------
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
        using DBPos = Engine::ECS::DoubleBuffered<TestPosition>;
        DBPos*                  db_pos = ecs_field(it, DBPos, 0);
        const TestMoveIntent* intent   = ecs_field(it, const TestMoveIntent, 1);
        DEBUG_ASSERT(db_pos  != nullptr);
        DEBUG_ASSERT(intent  != nullptr);
        for (int i = 0; i < it->count; ++i) {
            db_pos[i].write.pos =
                db_pos[i].read.pos + intent[i].dir * (k_speed * it->delta_time);
        }
    };
    const ecs_entity_t sys = ecs_system_init(world, &sys_desc);
    REQUIRE(sys != 0);

    // --- Register read←write sync on Phase_StateSync ----------------
    Engine::ECS::RegisterDoubleBufferSync<TestPosition>(world, id_DBPos);

    // --- Spawn the entity with known initial state -----------------------
    ecs_entity_desc_t player_desc = {};
    player_desc.name = "TestPlayer";
    const ecs_entity_t player = ecs_entity_init(world, &player_desc);
    REQUIRE(player != 0);

    Engine::ECS::DoubleBuffered<TestPosition> init_pos = {};
    init_pos.read.pos  = Engine::Math::ScryVec2(0.0f, 0.0f);
    init_pos.write.pos = Engine::Math::ScryVec2(0.0f, 0.0f);
    ecs_set_id(world, player, id_DBPos, sizeof(init_pos), &init_pos);

    TestMoveIntent init_intent = {};
    init_intent.dir = Engine::Math::ScryVec2(1.0f, 0.0f); // pure +X
    ecs_set_id(world, player, id_Intent, sizeof(init_intent), &init_intent);

    // --- Advance the pipeline exactly one tick ---------------------------
    const bool ticked = ecs_progress(world, k_delta_time);
    REQUIRE(ticked);

    // --- Assert: Write buffer must hold the mathematically correct result -
    const void* raw = ecs_get_id(world, player, id_DBPos);
    REQUIRE(raw != nullptr);
    const auto* result =
        static_cast<const Engine::ECS::DoubleBuffered<TestPosition>*>(raw);

    // write.pos set by TestMoveSystem
    REQUIRE(result->write.pos.x() == Catch::Approx(k_expected_x));
    REQUIRE(result->write.pos.y() == Catch::Approx(k_expected_y));

    // read.pos must have been synced from write.pos by SyncDoubleBuffer
    REQUIRE(result->read.pos.x()  == Catch::Approx(k_expected_x));
    REQUIRE(result->read.pos.y()  == Catch::Approx(k_expected_y));

    // --- Teardown: destroy ECS world -------------------------------------
    // ecs_fini releases every Flecs allocation routed through our mimalloc
    // OS-API hooks.  If anything leaks internally, Flecs will log it.
    ecs_fini(world);

    // --- Post-teardown heap sanity check ---------------------------------
    // A fresh allocation after world teardown proves the mimalloc heap is
    // intact and still functioning; a corrupted heap would crash or assert here.
    void* post_alloc = Engine::Memory::AllocInDll(64);
    REQUIRE(post_alloc != nullptr);
    REQUIRE(Engine::Memory::IsUsingMimalloc(post_alloc));
    Engine::Memory::FreeInDll(post_alloc);
}
