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
ENGINE_API extern ecs_entity_t Phase_Cleanup;     // Bulk-zeros all registered intent component arrays

// ── Intent component registry ─────────────────────────────────────────────
// Metadata tag applied to component TYPE entities whose instances are
// one-shot intents. Intent structs must begin with `uint8_t active` so the
// cleanup system can zero the entire contiguous array with a single memset.
ENGINE_API extern ecs_entity_t IsIntent;

// ── Lifecycle ─────────────────────────────────────────────────────────────
// Build and activate the custom pipeline. Must be called from CreateWorld()
// after ecs_init().
ENGINE_API void InitPipeline(ecs_world_t* world);

// Register an intent component for automatic bulk-zero cleanup each frame.
// Creates a Phase_Cleanup system that memsets every instance of comp_id to 0,
// resetting the `active` flag and payload without any archetype mutation.
ENGINE_API void RegisterIntentComponent(ecs_world_t* world, ecs_entity_t comp_id);

} // namespace Pipeline
} // namespace Engine
