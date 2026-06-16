#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <engine/engine.h>
#include <engine/ecs.h>
#include <engine/pipeline.h>
#include <engine/memory.h>
#include <engine/math.h>

#include <cassert>
#include <cstdio>
#include <flecs.h>
#include <cglm/cglm.h>

TEST_CASE("Headless Flecs world boots with custom pipeline phases", "[ecs]") {
    std::printf("[test] Starting headless ECS world boot test...\n");

    ScryECS_InitOSAPI();
    ecs_world_t* world = ScryECS_CreateWorld();
    REQUIRE(world != nullptr);

    ScryPipeline_Init(world);

    REQUIRE(ScryPhase_Input       != 0);
    REQUIRE(ScryPhase_Intent      != 0);
    REQUIRE(ScryPhase_StateUpdate != 0);
    REQUIRE(ScryPhase_StateSync   != 0);
    REQUIRE(ScryPhase_React       != 0);
    REQUIRE(ScryPhase_Cleanup     != 0);
    REQUIRE(ScryIsIntent          != 0);

    ecs_fini(world);
    ScryECS_ShutdownOSAPI();
}
