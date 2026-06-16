#include <engine/ecs.h>
#include <engine/pipeline.h>
#include <engine/transform.h>
#include <engine/camera.h>
#include <engine/spatial.h>
#include <engine/memory.h>
#include <engine/engine.h>
#include <engine/threading.h>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <thread>

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
    EngineLog("[ECS] Initializing OS API...");
    ecs_os_set_api_defaults();

    const int hw = static_cast<int>(std::thread::hardware_concurrency());
    const int pool_threads = (hw > 1) ? hw - 1 : 1;
    Threading::Init(pool_threads);
    Threading::SetFlecsOSAPI();
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
    EngineLog("[ECS] Shutting down OS API...");
    Threading::Shutdown();
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

    const int32_t hw = static_cast<int32_t>(std::thread::hardware_concurrency());
    ecs_set_threads(world, (hw > 1) ? hw : 1);
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "[ECS] World created (%d Flecs worker threads)", (hw > 1) ? hw : 1);
        EngineLog(buf);
    }
    Pipeline::InitPipeline(world);
    EngineLog("[ECS] Pipeline ready");
    Transform::Init(world);
    Camera::Init(world);
    Spatial::Init(world);

    return world;
}

} // namespace ECS
} // namespace Engine
