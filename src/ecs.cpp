#include <engine/ecs.hpp>
#include <engine/pipeline.hpp>
#include <engine/transform.hpp>
#include <engine/camera.hpp>
#include <engine/memory.hpp>
#include <mimalloc.h>
#include <cstring>
#include <libassert/assert.hpp>

namespace Engine {
namespace ECS {

static void* FlecsMalloc(ecs_size_t size) {
    return mi_malloc(static_cast<size_t>(size));
}

static void FlecsFree(void* ptr) {
    if (ptr) mi_free(ptr);
}

static void* FlecsCalloc(ecs_size_t size) {
    return mi_calloc(1, static_cast<size_t>(size));
}

static void* FlecsRealloc(void* ptr, ecs_size_t size) {
    return mi_realloc(ptr, static_cast<size_t>(size));
}

static char* FlecsStrdup(const char* str) {
    if (str == nullptr) return nullptr;
    const size_t len = std::strlen(str) + 1;
    char* copy = static_cast<char*>(mi_malloc(len));
    if (copy) std::memcpy(copy, str, len);
    return copy;
}

void InitOSAPI() {
    ecs_os_set_api_defaults();
    ecs_os_api_t api = ecs_os_api;

    api.malloc_  = FlecsMalloc;
    api.free_    = FlecsFree;
    api.calloc_  = FlecsCalloc;
    api.realloc_ = FlecsRealloc;
    api.strdup_  = FlecsStrdup;

    ecs_os_set_api(&api);
}

void ShutdownOSAPI() {
}

ecs_world_t* CreateWorld() {
    InitOSAPI();

    ecs_world_t* world = ecs_init();
    DEBUG_ASSERT(world != nullptr);
    if (world == nullptr) {
        EngineLog("[ECS] FATAL: ecs_init() returned null");
        return nullptr;
    }

#ifndef NDEBUG
    EngineLog("[ECS] World created");
#endif

    Pipeline::InitPipeline(world);

#ifndef NDEBUG
    EngineLog("[ECS] Pipeline ready");
#endif

    Transform::Init(world);
    Camera::Init(world);

    return world;
}

} // namespace ECS
} // namespace Engine
