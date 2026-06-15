#include <engine/ecs.hpp>
#include <engine/pipeline.hpp>
#include <engine/transform.hpp>
#include <engine/camera.hpp>
#include <engine/memory.hpp>
#include <cassert>
#include <cstring>
#include <cstdlib>

namespace Engine {
namespace ECS {

void InitOSAPI() {
    // Use Flecs default allocators (C malloc/free)
    ecs_os_set_api_defaults();
}

void ShutdownOSAPI() {
}

ecs_world_t* CreateWorld() {
    InitOSAPI();

    ecs_world_t* world = ecs_init();
    assert(world != nullptr);
    if (!world) {
        EngineLog("[ECS] FATAL: ecs_init() returned null");
        return nullptr;
    }

    EngineLog("[ECS] World created");
    Pipeline::InitPipeline(world);
    EngineLog("[ECS] Pipeline ready");
    Transform::Init(world);
    Camera::Init(world);

    return world;
}

} // namespace ECS
} // namespace Engine
