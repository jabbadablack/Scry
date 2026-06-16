#pragma once
#include <engine/engine.h>
#include <flecs.h>

namespace Engine {
namespace Pipeline {

ENGINE_API extern ecs_entity_t Phase_Input;
ENGINE_API extern ecs_entity_t Phase_Intent;
ENGINE_API extern ecs_entity_t Phase_StateUpdate;
ENGINE_API extern ecs_entity_t Phase_StateSync;
ENGINE_API extern ecs_entity_t Phase_React;
ENGINE_API extern ecs_entity_t Phase_Cleanup;
ENGINE_API extern ecs_entity_t IsIntent;

/**
 * @brief Constructs the engine's custom ISR pipeline.
 *
 * Hello! This function builds the specialized execution pipeline
 * (Input -> Intent -> State -> React -> Cleanup) that powers our engine.
 * It's the blueprint for how we process each frame!
 *
 * @param world The Flecs world to install the pipeline into.
 *
 * @example
 * Engine::Pipeline::InitPipeline(world);
 */
ENGINE_API void InitPipeline(ecs_world_t* world);

/**
 * @brief Hooks up a component to our intent-based system.
 *
 * Greetings! If you have a component that represents a user's intent
 * (like "wants to move"), call this to make sure the pipeline knows
 * how to handle it properly.
 *
 * @param world The Flecs world.
 * @param comp_id The entity ID of the component you're registering.
 *
 * @example
 * ecs_entity_t my_intent = ecs_component_init(world, ...);
 * Engine::Pipeline::RegisterIntentComponent(world, my_intent);
 */
ENGINE_API void RegisterIntentComponent(ecs_world_t* world, ecs_entity_t comp_id);

} // namespace Pipeline
} // namespace Engine
