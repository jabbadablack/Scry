/*
 * examples/sandbox/main.cpp
 *
 * Demonstrates the Intent-State-Reactor (ISR) lifecycle of the Scry engine
 * using a simple combat tick:
 *
 *   Intent phase  — DamageIntent component is placed on the Player entity
 *   State phase   — DamageSystem (ScryPhase_StateUpdate) subtracts the intent
 *                   from Health via ecs_set_id, then explicitly removes it
 *   Cleanup phase — RegisterIntentComponent ensures leftover DamageIntents are
 *                   bulk-stripped at the end of every frame
 *   Reactor       — ecs_observer_desc_t watching EcsOnSet on Health fires
 *                   whenever Health changes; no event bus required
 *
 * Logging note: game code must call ScryLog() rather than LOG_INFO() directly.
 * Quill is a static library — the engine DLL and each binary carry separate
 * copies of the Quill globals.  Only the DLL's copy is initialised by ScryRun;
 * calling LOG_INFO() from EXE code before its own quill::start() triggers the
 * _g_root_logger nullptr assertion.  ScryLog() forwards through the DLL's live
 * Quill instance and is safe to call from any thread or ECS callback.
 */

#include <scry/scry.h>
#include <scry/scry_pipeline.hpp>   // ScryPhase_StateUpdate, RegisterIntentComponent
#include <scry/scry_json.hpp>       // LoadProjectConfig (loads the input plugin)
#include <scry/scry_plugin.hpp>     // UnloadPlugins
#include <libassert/assert.hpp>
#include <flecs.h>
#include <cstdio>                   // snprintf

// ── Component types ───────────────────────────────────────────────────────────
// Defined at file scope so non-capturing lambda callbacks can use sizeof /
// alignof and ecs_field<T> without captures.

struct Health {
    float value;
};

struct DamageIntent {
    float amount;
};

// ── Per-run application state ─────────────────────────────────────────────────
// Stored via ScryAppConfig::user_data so OnUpdate can reach entities and
// component IDs.  Populated in OnInit.

struct GameState {
    ecs_world_t* world     = nullptr;
    ecs_entity_t id_Health = 0;
    ecs_entity_t id_Damage = 0;
    ecs_entity_t player    = 0;
};

static GameState g_state;
static uint32_t  g_frame = 0;

static constexpr uint32_t k_damage_frames = 5u;
static constexpr uint32_t k_total_frames  = 10u;
static constexpr float    k_damage_amount = 10.0f;

// ── Lifecycle callbacks ───────────────────────────────────────────────────────

static void OnInit(ScryContext* ctx) {
    DEBUG_ASSERT(ctx != nullptr);

    // Load plugins declared in scry_project.json (includes the input plugin).
    const bool proj_ok = Scry::JSON::LoadProjectConfig(ctx, "scry_project.json");
    DEBUG_ASSERT(proj_ok);
    (void)proj_ok;

    GameState*   gs    = static_cast<GameState*>(ScryGetUserData(ctx));
    ecs_world_t* world = ScryGetWorld(ctx);
    gs->world          = world;

    // ── Health component ──────────────────────────────────────────────────────
    {
        ecs_entity_desc_t e    = {};
        e.name                 = "Health";
        ecs_component_desc_t c = {};
        c.entity               = ecs_entity_init(world, &e);
        c.type.size            = sizeof(Health);
        c.type.alignment       = alignof(Health);
        gs->id_Health          = ecs_component_init(world, &c);
        DEBUG_ASSERT(gs->id_Health != 0);
    }

    // ── DamageIntent component ────────────────────────────────────────────────
    {
        ecs_entity_desc_t e    = {};
        e.name                 = "DamageIntent";
        ecs_component_desc_t c = {};
        c.entity               = ecs_entity_init(world, &e);
        c.type.size            = sizeof(DamageIntent);
        c.type.alignment       = alignof(DamageIntent);
        gs->id_Damage          = ecs_component_init(world, &c);
        DEBUG_ASSERT(gs->id_Damage != 0);
    }

    // Tag DamageIntent so ScryPhase_Cleanup bulk-strips it from all entities
    // at the end of every frame, preventing stale intents from leaking.
    Scry::Pipeline::RegisterIntentComponent(world, gs->id_Damage);

    // ── DamageSystem — State phase ────────────────────────────────────────────
    // Runs in ScryPhase_StateUpdate.  For each entity with both Health and
    // DamageIntent it subtracts the damage amount and writes the new Health
    // back via ecs_set_id.  Writing through ecs_set_id (not a direct field
    // pointer write) is what raises EcsOnSet and causes the Reactor to fire.
    // The consumed intent is also explicitly removed here; RegisterIntentComponent
    // above ensures the Cleanup phase handles any remaining stragglers.
    {
        ecs_entity_desc_t e  = {};
        e.name               = "DamageSystem";
        ecs_entity_t sys_ent = ecs_entity_init(world, &e);
        ecs_add_pair(world, sys_ent, EcsDependsOn,
                     Scry::Pipeline::ScryPhase_StateUpdate);

        ecs_system_desc_t s    = {};
        s.entity               = sys_ent;
        s.query.terms[0].id    = gs->id_Health;          // term 0 — read/write
        s.query.terms[1].id    = gs->id_Damage;          // term 1 — read-only
        s.query.terms[1].inout = EcsIn;
        s.callback = [](ecs_iter_t* it) {
            Health*             hp  = ecs_field(it, Health, 0);
            const DamageIntent* dmg = ecs_field(it, DamageIntent, 1);
            for (int i = 0; i < it->count; ++i) {
                Health next = hp[i];
                next.value -= dmg[i].amount;
                // Must go through ecs_set_id — a direct pointer write does not
                // raise EcsOnSet and would leave the Reactor observer silent.
                ecs_set_id(it->world, it->entities[i],
                           it->ids[0], sizeof(Health), &next);
                // Explicit consumption; Cleanup phase is the safety net.
                ecs_remove_id(it->world, it->entities[i], it->ids[1]);
            }
        };

        const ecs_entity_t sys = ecs_system_init(world, &s);
        DEBUG_ASSERT(sys != 0);
    }

    // ── HealthReactor — Reactor (EcsOnSet observer) ───────────────────────────
    // Flecs observers are event-driven, not phase-ordered.  This one fires
    // synchronously at the merge point after DamageSystem's ecs_set_id calls
    // are applied — no event bus, no manual notification needed.
    //
    // The callback uses ScryLog() rather than LOG_INFO() directly because Quill
    // is statically linked: only the engine DLL's copy is initialised by
    // ScryRun.  ScryLog() routes the call through that live DLL instance and
    // is safe on any thread, including Flecs worker threads.
    {
        ecs_entity_desc_t e   = {};
        e.name                = "HealthReactor";
        ecs_observer_desc_t o = {};
        o.entity              = ecs_entity_init(world, &e);
        o.query.terms[0].id   = gs->id_Health;
        o.events[0]           = EcsOnSet;
        o.callback = [](ecs_iter_t* it) {
            const Health* hp = ecs_field(it, Health, 0);
            char buf[64];
            for (int i = 0; i < it->count; ++i) {
                snprintf(buf, sizeof(buf),
                         "[React] HealthReactor fired — health is now %.1f",
                         static_cast<double>(hp[i].value));
                ScryLog(buf);
            }
        };

        const ecs_entity_t reactor = ecs_observer_init(world, &o);
        DEBUG_ASSERT(reactor != 0);
    }

    // ── Spawn Player entity ───────────────────────────────────────────────────
    // ecs_set_id on Health fires the HealthReactor immediately because the world
    // is not in a deferred staging context here (OnInit runs outside ecs_progress).
    {
        ecs_entity_desc_t e = {};
        e.name              = "Player";
        gs->player          = ecs_entity_init(world, &e);

        Health hp_init      = { 100.0f };
        ecs_set_id(world, gs->player, gs->id_Health,
                   sizeof(hp_init), &hp_init);

        // Queue the first DamageIntent for the very first ECS tick.
        DamageIntent dmg_init = { k_damage_amount };
        ecs_set_id(world, gs->player, gs->id_Damage,
                   sizeof(dmg_init), &dmg_init);
    }

    char buf[128];
    snprintf(buf, sizeof(buf),
             "[Init] ISR demo ready — %.0f damage/frame for %u frames",
             static_cast<double>(k_damage_amount), k_damage_frames);
    ScryLog(buf);
}

static void OnUpdate(ScryContext* ctx, float dt) {
    DEBUG_ASSERT(ctx != nullptr);
    (void)dt;

    GameState* gs = static_cast<GameState*>(ScryGetUserData(ctx));
    ++g_frame;

    // Re-queue a DamageIntent each frame while g_frame <= k_damage_frames so
    // the full ISR cycle (Intent → State → Cleanup → Reactor) repeats visibly.
    // OnUpdate runs AFTER ecs_progress; the intent lands in time for the NEXT
    // frame's DamageSystem pass.
    if (g_frame <= k_damage_frames) {
        DamageIntent dmg = { k_damage_amount };
        ecs_set_id(gs->world, gs->player, gs->id_Damage, sizeof(dmg), &dmg);
    }

    if (g_frame >= k_total_frames) {
        RequestEngineExit(ctx);
    }
}

static void OnShutdown(ScryContext* ctx) {
    DEBUG_ASSERT(ctx != nullptr);
    (void)ctx;

    char buf[64];
    snprintf(buf, sizeof(buf),
             "[Shutdown] ISR demo complete — %u frames rendered", g_frame);
    ScryLog(buf);

    Scry::Plugin::UnloadPlugins();
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    ScryAppConfig config  = {};
    config.title          = "Scry ISR Demo";
    config.window_width   = 800;
    config.window_height  = 600;
    config.OnInit         = OnInit;
    config.OnUpdate       = OnUpdate;
    config.OnShutdown     = OnShutdown;
    config.user_data      = &g_state;

    return ScryRun(&config);
}
