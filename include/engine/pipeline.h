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

ENGINE_API void InitPipeline(ecs_world_t* world);
ENGINE_API void RegisterIntentComponent(ecs_world_t* world, ecs_entity_t comp_id);

} // namespace Pipeline
} // namespace Engine
