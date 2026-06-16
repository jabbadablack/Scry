#pragma once
#include <engine/engine.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ecs_world_t;

ENGINE_API extern uint64_t ScryPhase_Input;
ENGINE_API extern uint64_t ScryPhase_Intent;
ENGINE_API extern uint64_t ScryPhase_StateUpdate;
ENGINE_API extern uint64_t ScryPhase_StateSync;
ENGINE_API extern uint64_t ScryPhase_React;
ENGINE_API extern uint64_t ScryPhase_Cleanup;
ENGINE_API extern uint64_t ScryIsIntent;

/**
 * @brief Constructs the engine's custom ISR pipeline.
 */
ENGINE_API void ScryPipeline_Init(struct ecs_world_t* world);

/**
 * @brief Hooks up a component to our intent-based system.
 */
ENGINE_API void ScryPipeline_RegisterIntentComponent(struct ecs_world_t* world, uint64_t comp_id);

#ifdef __cplusplus
}
#endif
