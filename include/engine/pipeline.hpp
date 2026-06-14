#pragma once
#include <engine/engine.h>
#include <flecs.h>

namespace Engine {
namespace Pipeline {

// ── Custom pipeline phases ────────────────────────────────────────────────
// All entities are zero until InitPipeline() is called from CreateWorld().
// The phases execute in the order listed below (enforced by EcsDependsOn).

ENGINE_API extern ecs_entity_t Phase_Input;       // Reads raw SDL3 input state
ENGINE_API extern ecs_entity_t Phase_Intent;      // Converts input/AI into Intent components
ENGINE_API extern ecs_entity_t Phase_StateUpdate; // Core logic: evaluates Intents + State
ENGINE_API extern ecs_entity_t Phase_StateSync;   // Flips DoubleBuffered<T> write -> read
ENGINE_API extern ecs_entity_t Phase_React;       // Audio, VFX, and other reactions
ENGINE_API extern ecs_entity_t Phase_Cleanup;     // Bulk-strips all IsIntent components

// ── Intent component registry ─────────────────────────────────────────────
// Tag applied to component TYPE entities that represent one-shot intents.
// The Cleanup system removes every instance of each tagged type from all
// game entities at the end of each frame, preventing stale intent leakage.
//
// Usage (after registering your component):
//   ecs_add(world, ecs_id(MyIntentComp), Engine::Pipeline::IsIntent);
ENGINE_API extern ecs_entity_t IsIntent;

// ── Lifecycle ─────────────────────────────────────────────────────────────
// Build and activate the custom pipeline. Must be called from CreateWorld()
// after ecs_init() and the FlecsMeta module import.
ENGINE_API void InitPipeline(ecs_world_t* world);

// Convenience wrapper: tags comp_id with IsIntent so the cleanup system
// removes all instances of that component at the end of every frame.
ENGINE_API void RegisterIntentComponent(ecs_world_t* world, ecs_entity_t comp_id);

} // namespace Pipeline
} // namespace Engine
