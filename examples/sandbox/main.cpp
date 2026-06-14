/*
 * examples/sandbox/main.cpp
 *
 * Demonstrates the pure ISR (Intent-State-Reactor) lifecycle of the engine.
 * ALL per-frame logic lives in ECS systems registered during OnInit.
 */

#include <engine/engine.h>
#include <engine/pipeline.hpp>   // Phase_*, IsIntent
#include <engine/ecs.hpp>        // DoubleBuffered
#include <engine/json.hpp>       // LoadProjectConfig
#include <engine/plugin.hpp>     // UnloadPlugins
#include <libassert/assert.hpp>
#include <flecs.h>
#include <cstdio>                // snprintf

// Quill logging (application layer)
#define QUILL_ROOT_LOGGER_ONLY
#include <quill/Quill.h>

// ── Component types ───────────────────────────────────────────────────────────

struct Health       { float value; };
struct DamageIntent { float amount; };
struct Target {};

static constexpr float k_damage_amount = 10.0f;

// ── Logging callback ──────────────────────────────────────────────────────────

static void AppLog(const char* msg) {
    LOG_INFO("{}", msg);
}

// ── Lifecycle callbacks ───────────────────────────────────────────────────────

static void OnInit(Context* ctx) {
    DEBUG_ASSERT(ctx != nullptr);

    // Loads project.json from the executable directory by default.
    Engine::JSON::LoadProjectConfig(ctx, nullptr);

    ecs_world_t* world = GetWorld(ctx);

    // ── Health component ──────────────────────────────────────────────────────
    ecs_entity_t id_Health;
    {
        ecs_entity_desc_t e    = {};
        e.name                 = "DoubleBuffered<Health>";
        ecs_component_desc_t c = {};
        c.entity               = ecs_entity_init(world, &e);
        c.type.size            = sizeof(Engine::ECS::DoubleBuffered<Health>);
        c.type.alignment       = alignof(Engine::ECS::DoubleBuffered<Health>);
        id_Health              = ecs_component_init(world, &c);
        DEBUG_ASSERT(id_Health != 0);
    }
    
    // Register sync system
    Engine::ECS::RegisterDoubleBufferSync<Health>(world, id_Health);

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

    // ── Target relationship ───────────────────────────────────────────────────
    ecs_entity_t id_Target;
    {
        ecs_entity_desc_t e = {};
        e.name              = "Target";
        id_Target           = ecs_entity_init(world, &e);
        DEBUG_ASSERT(id_Target != 0);
    }

    // ── DamageQueueSystem — Intent phase ──────────────────────────────────────
    {
        ecs_entity_desc_t e        = {};
        e.name                     = "DamageQueueSystem";
        const ecs_entity_t sys_ent = ecs_entity_init(world, &e);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_Intent);

        ecs_system_desc_t s    = {};
        s.entity               = sys_ent;
        s.query.terms[0].id    = id_Health;
        s.query.terms[0].inout = EcsIn;
        s.callback = [](ecs_iter_t* it) {
            const Engine::ECS::DoubleBuffered<Health>* hp = ecs_field(it, Engine::ECS::DoubleBuffered<Health>, 0);
            const ecs_entity_t id_dmg = ecs_lookup(it->world, "DamageIntent");
            const ecs_entity_t id_isi = ecs_lookup(it->world, "IsIntent");
            const ecs_entity_t id_trg = ecs_lookup(it->world, "Target");

            for (int i = 0; i < it->count; ++i) {
                if (hp[i].read.value > 0.0f) {
                    ecs_entity_t intent = ecs_new(it->world);
                    ecs_add_id(it->world, intent, id_isi);
                    DamageIntent dmg = { k_damage_amount };
                    ecs_set_id(it->world, intent, id_dmg, sizeof(dmg), &dmg);
                    ecs_add_pair(it->world, intent, id_trg, it->entities[i]);
                }
            }
        };

        ecs_system_init(world, &s);
    }

    // ── DamageSystem — State phase ────────────────────────────────────────────
    {
        ecs_entity_desc_t e        = {};
        e.name                     = "DamageSystem";
        const ecs_entity_t sys_ent = ecs_entity_init(world, &e);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_StateUpdate);

        ecs_system_desc_t s    = {};
        s.entity               = sys_ent;
        s.query.terms[0].id    = id_Damage;
        s.query.terms[1].id    = ecs_pair(id_Target, id_Health);
        s.query.terms[1].src.id = EcsCascade;
        s.callback = [](ecs_iter_t* it) {
            const DamageIntent* dmg = ecs_field(it, DamageIntent, 0);
            Engine::ECS::DoubleBuffered<Health>* hp = ecs_field(it, Engine::ECS::DoubleBuffered<Health>, 1);
            for (int i = 0; i < it->count; ++i) {
                hp[i].write.value -= dmg[i].amount;
            }
        };

        ecs_system_init(world, &s);
    }

    // ── HealthReactorSystem — React phase ─────────────────────────────────────
    {
        ecs_entity_desc_t e        = {};
        e.name                     = "HealthReactorSystem";
        const ecs_entity_t sys_ent = ecs_entity_init(world, &e);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_React);

        ecs_system_desc_t s    = {};
        s.entity               = sys_ent;
        s.query.terms[0].id    = id_Health;
        s.query.terms[0].inout = EcsIn;
        s.callback = [](ecs_iter_t* it) {
            const Engine::ECS::DoubleBuffered<Health>* hp = ecs_field(it, Engine::ECS::DoubleBuffered<Health>, 0);
            char buf[64];
            for (int i = 0; i < it->count; ++i) {
                if (hp[i].read.value != hp[i].write.value) {
                    snprintf(buf, sizeof(buf), "[React] health %f -> %f", 
                             static_cast<double>(hp[i].read.value), 
                             static_cast<double>(hp[i].write.value));
                    EngineLog(buf);
                }
            }
        };

        ecs_system_init(world, &s);
    }

    // ── Spawn Player ──────────────────────────────────────────────────────────
    {
        ecs_entity_desc_t e       = {};
        e.name                    = "Player";
        const ecs_entity_t player = ecs_entity_init(world, &e);

        Engine::ECS::DoubleBuffered<Health> hp_init = {{100.0f}, {100.0f}};
        ecs_set_id(world, player, id_Health, sizeof(hp_init), &hp_init);
    }

    EngineLog("[Init] ISR demo ready — health will deplete to 0 over ~10 frames.");
}

static void OnShutdown(Context* ctx) {
    (void)ctx;
    EngineLog("[Shutdown] ISR demo complete");
    Engine::Plugin::UnloadPlugins();
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    // Initialize Quill in the application layer
    std::shared_ptr<quill::Handler> log_handler = quill::stdout_handler();
    log_handler->set_pattern("%(ascii_time) [%(thread)] %(fileline:<28) LOG_%(level_name) %(message)");
    quill::Config log_cfg;
    log_cfg.default_handlers.push_back(log_handler);
    quill::configure(log_cfg);
    quill::start();

    AppConfig config = {};
    config.title         = "Engine ISR Demo";
    config.window_width  = 800;
    config.window_height = 600;
    config.OnInit        = OnInit;
    config.OnShutdown    = OnShutdown;
    config.OnLog         = AppLog;
    config.global_memory_pool_size = 1024 * 1024;

    const EngineError err = EngineRun(&config);
    if (err != SUCCESS) {
        fprintf(stderr, "[Main] Engine failed to start with error code: %d\n", (int)err);
        return 1;
    }

    return 0;
}
