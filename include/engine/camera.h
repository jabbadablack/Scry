#pragma once
#include <engine/engine.h>
#include <components/camera.h>
#include <flecs.h>

namespace Engine {
namespace Camera {

ENGINE_API extern ecs_entity_t id_Camera;

/**
 * @brief Hey there! This friendly function sets up the camera system in your ECS world.
 * 
 * It's the first thing you should call if you want to see anything in your game!
 * 
 * @param world The Flecs world where the camera magic will happen.
 * 
 * @example
 * ecs_world_t* my_world = ecs_init();
 * Engine::Camera::Init(my_world);
 */
ENGINE_API void Init(ecs_world_t* world);

} // namespace Camera
} // namespace Engine
