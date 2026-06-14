/*
 * examples/sandbox/main.cpp
 *
 * Demonstrates the pure ISR (Intent-State-Reactor) lifecycle of the Scry engine.
 * ALL per-frame logic lives in ECS systems registered during OnInit; there is no
 * OnUpdate callback.  The 6-phase pipeline drives everything each frame:
 *
 *   Intent phase  — DamageQueueSystem adds DamageIntent to entities with
 *                   Health > 0, re-queuing it every frame automatically.
 *   State phase   — DamageSystem subtracts DamageIntent from Health via
 *                   ecs_set_id (required to raise EcsOnSet) and removes the
 *                   consumed intent.
 *   Cleanup phase — RegisterIntentComponent bulk-strips any leftover intents.
 *   Reactor       — HealthReactor (EcsOnSet observer) logs every Health change
 *                   with zero manual dispatch.
 *
 * After 10 frames Health reaches 0.  DamageQueueSystem stops queuing (health
 * is no longer > 0) and the demo idles silently until the user closes the
 * window (close button or Escape).
 *
 * Logging note: game code must call ScryLog() rather than LOG_INFO() directly.
 * Quill is statically linked; only the engine DLL's copy is initialised by
 * ScryRun.  ScryLog() routes through that live instance and is thread-safe.
 */

#include <scry/scry.h>
#include <scry/scry_pipeline.hpp>   // ScryPhase_*, RegisterIntentComponent
#include <scry/scry_json.hpp>       // LoadProjectConfig
#include <scry/scry_plugin.hpp>     // UnloadPlugins
#include <libassert/assert.hpp>
#include <flecs.h>
#include <cstdio>                   // snprintf

// ── Component types ───────────────────────────────────────────────────────────
// File-scope so non-capturing lambda callbacks can use sizeof/ecs_field<T>.

struct Health       { float value; };
struct DamageIntent { float amount; };

static constexpr float k_damage_amount = 10.0f;

// ── Lifecycle callbacks ───────────────────────────────────────────────────────

static void OnInit(ScryContext* ctx) {
    DEBUG_ASSERT(ctx != nullptr);

    const bool proj_ok = Scry::JSON::LoadProjectConfig(ctx, "scry_project.json");
    DEBUG_ASSERT(proj_ok);
    (void)proj_ok;

    ecs_world_t* world = ScryGetWorld(ctx);

    // ── Health component ──────────────────────────────────────────────────────
    ecs_entity_t id_Health;
    {
        ecs_entity_desc_t e    = {};
        e.name                 = "Health";
        ecs_component_desc_t c = {};
        c.entity               = ecs_entity_init(world, &e);
        c.type.size            = sizeof(Health);
        c.type.alignment       = alignof(Health);
        id_Health              = ecs_component_init(world, &c);
        DEBUG_ASSERT(id_Health != 0);
    }

    // ── DamageIntent component ────────────────────────────────────────────────
    ecs_entity_t id_Damage;
    {
        ecs_entity_desc_t e    = {};
        e.name                 = "DamageIntent";
        ecs_component_desc_t c = {};
        c.entity               = ecs_entity_init(world, &e);
        c.type.size            = sizeof(DamageIntent);
        c.type.alignment       = alignof(DamageIntent);
        id_Damage              = ecs_component_init(world, &c);
        DEBUG_ASSERT(id_Damage != 0);
    }

    // Register DamageIntent as an intent: Cleanup phase bulk-strips it from all
    // entities at the end of every frame so stale intents never leak forward.
    Scry::Pipeline::RegisterIntentComponent(world, id_Damage);

    // ── DamageQueueSystem — Intent phase ──────────────────────────────────────
    // Runs first in the pipeline each frame.  For every entity whose Health > 0
    // it writes a DamageIntent, re-triggering the ISR cycle automatically.
    // ecs_set_id inside a running system is deferred; the intent is committed
    // before StateUpdate runs so DamageSystem sees it in the same frame.
    {
        ecs_entity_desc_t e        = {};
        e.name                     = "DamageQueueSystem";
        const ecs_entity_t sys_ent = ecs_entity_init(world, &e);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Scry::Pipeline::ScryPhase_Intent);

        ecs_system_desc_t s    = {};
        s.entity               = sys_ent;
        s.query.terms[0].id    = id_Health;
        s.query.terms[0].inout = EcsIn;
        // id_Damage passed via ctx since non-capturing lambdas have no captures.
        s.ctx = reinterpret_cast<void*>(static_cast<uintptr_t>(id_Damage));
        s.callback = [](ecs_iter_t* it) {
            const Health*      hp     = ecs_field(it, Health, 0);
            const ecs_entity_t id_dmg = static_cast<ecs_entity_t>(
                reinterpret_cast<uintptr_t>(it->ctx));
            DamageIntent dmg = { k_damage_amount };
            for (int i = 0; i < it->count; ++i) {
                if (hp[i].value > 0.0f) {
                    ecs_set_id(it->world, it->entities[i],
                               id_dmg, sizeof(dmg), &dmg);
                }
            }
        };

        const ecs_entity_t sys = ecs_system_init(world, &s);
        DEBUG_ASSERT(sys != 0);
    }

    // ── DamageSystem — State phase ────────────────────────────────────────────
    // Consumes DamageIntent: subtracts the damage amount from Health via
    // ecs_set_id (raises EcsOnSet → triggers HealthReactor) then removes the
    // intent.  The Cleanup phase strips any stragglers.
    {
        ecs_entity_desc_t e        = {};
        e.name                     = "DamageSystem";
        const ecs_entity_t sys_ent = ecs_entity_init(world, &e);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Scry::Pipeline::ScryPhase_StateUpdate);

        ecs_system_desc_t s    = {};
        s.entity               = sys_ent;
        s.query.terms[0].id    = id_Health;
        s.query.terms[1].id    = id_Damage;
        s.query.terms[1].inout = EcsIn;
        s.callback = [](ecs_iter_t* it) {
            Health*             hp  = ecs_field(it, Health, 0);
            const DamageIntent* dmg = ecs_field(it, DamageIntent, 1);
            for (int i = 0; i < it->count; ++i) {
                Health next = hp[i];
                next.value -= dmg[i].amount;
                // ecs_set_id raises EcsOnSet; a direct pointer write does not.
                ecs_set_id(it->world, it->entities[i],
                           it->ids[0], sizeof(Health), &next);
                ecs_remove_id(it->world, it->entities[i], it->ids[1]);
            }
        };

        const ecs_entity_t sys = ecs_system_init(world, &s);
        DEBUG_ASSERT(sys != 0);
    }

    // ── HealthReactor — EcsOnSet observer ─────────────────────────────────────
    // Fires synchronously at the merge point after DamageSystem's ecs_set_id
    // calls are applied — zero-cost, zero-polling, no event bus required.
    {
        ecs_entity_desc_t e   = {};
        e.name                = "HealthReactor";
        ecs_observer_desc_t o = {};
        o.entity              = ecs_entity_init(world, &e);
        o.query.terms[0].id   = id_Health;
        o.events[0]           = EcsOnSet;
        o.callback = [](ecs_iter_t* it) {
            const Health* hp = ecs_field(it, Health, 0);
            char buf[64];
            for (int i = 0; i < it->count; ++i) {
                snprintf(buf, sizeof(buf),
                         "[React] health → %.1f",
                         static_cast<double>(hp[i].value));
                ScryLog(buf);
            }
        };

        const ecs_entity_t reactor = ecs_observer_init(world, &o);
        DEBUG_ASSERT(reactor != 0);
    }

    // ── Spawn Player with Health = 100 ────────────────────────────────────────
    // ecs_set_id here is outside ecs_progress (no deferred staging), so
    // HealthReactor fires immediately with the initial value of 100.
    {
        ecs_entity_desc_t e       = {};
        e.name                    = "Player";
        const ecs_entity_t player = ecs_entity_init(world, &e);

        Health hp_init = { 100.0f };
        ecs_set_id(world, player, id_Health, sizeof(hp_init), &hp_init);
    }

    ScryLog("[Init] ISR demo ready — health will deplete to 0 over ~10 frames."
            " Press Escape or close the window to exit.");
}

static void OnShutdown(ScryContext* ctx) {
    (void)ctx;
    ScryLog("[Shutdown] ISR demo complete");
    Scry::Plugin::UnloadPlugins();
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    ScryAppConfig config = {};
    config.title         = "Scry ISR Demo";
    config.window_width  = 800;
    config.window_height = 600;
    config.OnInit        = OnInit;
    config.OnShutdown    = OnShutdown;

    return ScryRun(&config);
}
