#pragma once
#include <engine/engine.h>
#include <engine/math.hpp>
#include <flecs.h>

namespace Engine {
namespace Camera {

struct Camera {
    Math::ScryVec3 position;
    float          pitch;
    float          yaw;
    float          view[16];
    float          proj[16];
};

ENGINE_API extern ecs_entity_t id_Camera;

ENGINE_API void Init(ecs_world_t* world);

} // namespace Camera
} // namespace Engine
