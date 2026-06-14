#pragma once
#include <scry/scry.h>
#include <flecs.h>

namespace Scry {
namespace Pipeline {

// ── Custom pipeline phases ────────────────────────────────────────────────
// All entities are zero until InitPipeline() is called from CreateWorld().
// The phases execute in the order listed below (enforced by EcsDependsOn).

SCRY_API extern ecs_entity_t ScryPhase_Input;       // Reads raw SDL3 input state
SCRY_API extern ecs_entity_t ScryPhase_Intent;      // Converts input/AI into Intent components
SCRY_API extern ecs_entity_t ScryPhase_StateUpdate; // Core logic: evaluates Intents + State
SCRY_API extern ecs_entity_t ScryPhase_StateSync;   // Flips DoubleBuffered<T> write -> read
SCRY_API extern ecs_entity_t ScryPhase_React;       // Audio, VFX, and other reactions
SCRY_API extern ecs_entity_t ScryPhase_Cleanup;     // Bulk-strips all IsIntent components

// ── Intent component registry ─────────────────────────────────────────────
// Tag applied to component TYPE entities that represent one-shot intents.
// The Cleanup system removes every instance of each tagged type from all
// game entities at the end of each frame, preventing stale intent leakage.
//
// Usage (after registering your component):
//   ecs_add(world, ecs_id(MyIntentComp), Scry::Pipeline::IsIntent);
SCRY_API extern ecs_entity_t IsIntent;

// ── Lifecycle ─────────────────────────────────────────────────────────────
// Build and activate the custom pipeline. Must be called from CreateWorld()
// after ecs_init() and the FlecsMeta module import.
SCRY_API void InitPipeline(ecs_world_t* world);

// Convenience wrapper: tags comp_id with IsIntent so the cleanup system
// removes all instances of that component at the end of every frame.
SCRY_API void RegisterIntentComponent(ecs_world_t* world, ecs_entity_t comp_id);

} // namespace Pipeline
} // namespace Scry
