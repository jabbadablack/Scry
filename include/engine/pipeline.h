#pragma once
#include <engine/engine.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ecs_world_t;

ENGINE_API extern uint64_t ScryPhase_Sense;    /* Gathers data: input, raycasts, blackboard      */
ENGINE_API extern uint64_t ScryPhase_Evaluate; /* Scores intents: AI brains, camera input         */
ENGINE_API extern uint64_t ScryPhase_React;    /* Translates intent to state: matrices, pathfind  */
ENGINE_API extern uint64_t ScryPhase_Resolve;  /* Commits state: GPU upload, physics integration  */
ENGINE_API extern uint64_t ScryPhase_Cleanup;  /* GC step: wipe intents, release cmd lists, reset arena */
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
