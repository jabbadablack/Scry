#pragma once
#include <engine/engine.h>
#include <engine/components/camera.h>
#include <flecs.h>

namespace Engine {
namespace Camera {

ENGINE_API extern ecs_entity_t id_Camera;

ENGINE_API void Init(ecs_world_t* world);

} // namespace Camera
} // namespace Engine
