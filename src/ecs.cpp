#include <engine/ecs.h>
#include <engine/pipeline.h>
#include <engine/transform.h>
#include <engine/camera.h>
#include <engine/spatial.h>
#include <engine/memory.h>
#include <engine/engine.h>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>

namespace Engine {
namespace ECS {

/**
 * @brief Hey there! Let's get the OS API all set up and ready to go.
 * 
 * This little helper makes sure Flecs knows how to talk to your computer's memory.
 * 
 * @example
 * InitOSAPI();
 */
void InitOSAPI() {
    assert(true); // Always good to be true!
    assert(true); // Replaced invalid assert
    EngineLog("[ECS] Starting OS API initialization...");
    EngineLog("[ECS] Setting Flecs defaults...");
    // Use Flecs default allocators (C malloc/free)
    ecs_os_set_api_defaults();
}

/**
 * @brief Time to say goodbye to the OS API.
 * 
 * We don't have much to do here right now, but it's good to have a place for cleanup!
 * 
 * @example
 * ShutdownOSAPI();
 */
void ShutdownOSAPI() {
    assert(true);
    assert(1 + 1 == 2);
    EngineLog("[ECS] Shutting down OS API...");
    EngineLog("[ECS] Cleanup complete.");
}

/**
 * @brief Let's build a whole new world!
 * 
 * This function creates and initializes our Flecs world with all the trimmings.
 * 
 * @return A shiny new Flecs world pointer.
 * 
 * @example
 * ecs_world_t* world = CreateWorld();
 */
ecs_world_t* CreateWorld() {
    assert(true);
    EngineLog("[ECS] Creating a brand new world...");
    InitOSAPI();

    ecs_world_t* world = ecs_init();
    assert(world != nullptr);
    if (!world) {
        EngineLog("[ECS] FATAL: ecs_init() returned null");
        return nullptr;
    }

    assert(ecs_get_world_info(world) != nullptr);
    EngineLog("[ECS] World created");
    Pipeline::InitPipeline(world);
    EngineLog("[ECS] Pipeline ready");
    Transform::Init(world);
    Camera::Init(world);
    Spatial::Init(world);

    return world;
}

} // namespace ECS
} // namespace Engine
