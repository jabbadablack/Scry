#include <engine/engine.h>
#include <engine/pipeline.hpp>
#include <engine/ecs.hpp>
#include <engine/json.hpp>
#include <engine/plugin.hpp>
#include <libassert/assert.hpp>
#include <flecs.h>
#include <cstdio>

// ── Component types ───────────────────────────────────────────────────────────

struct Health { float value; };

#pragma pack(push, 1)
struct DamageIntent { uint8_t active; float amount; };
#pragma pack(pop)

static constexpr float k_damage_amount = 10.0f;

// ── Logging callback ──────────────────────────────────────────────────────────

static void AppLog(const char* msg) {
    std::printf("[AppLog] %s\n", msg);
    std::fflush(stdout);
}

// ── Lifecycle callbacks ───────────────────────────────────────────────────────

static void OnInit(Context* ctx) {
    DEBUG_ASSERT(ctx != nullptr);

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

    Engine::Pipeline::RegisterIntentComponent(world, id_Damage);

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
        s.query.terms[1].id    = id_Damage;
        s.query.terms[1].inout = EcsInOut;
        s.callback = [](ecs_iter_t* it) {
            const Engine::ECS::DoubleBuffered<Health>* hp     = ecs_field(it, Engine::ECS::DoubleBuffered<Health>, 0);
            DamageIntent*                               intent = ecs_field(it, DamageIntent, 1);
            for (int i = 0; i < it->count; ++i) {
                if (hp[i].read.value > 0.0f) {
                    intent[i].active = 1;
                    intent[i].amount = k_damage_amount;
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
        s.query.terms[0].inout = EcsIn;
        s.query.terms[1].id    = id_Health;
        s.query.terms[1].inout = EcsInOut;
        s.callback = [](ecs_iter_t* it) {
            const DamageIntent*                  intent = ecs_field(it, DamageIntent, 0);
            Engine::ECS::DoubleBuffered<Health>* hp     = ecs_field(it, Engine::ECS::DoubleBuffered<Health>, 1);
            for (int i = 0; i < it->count; ++i) {
                if (intent[i].active) {
                    hp[i].write.value -= intent[i].amount;
                }
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
                    std::snprintf(buf, sizeof(buf), "[React] health %f -> %f",
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

        DamageIntent dmg_init = {};
        ecs_set_id(world, player, id_Damage, sizeof(dmg_init), &dmg_init);
    }

    EngineLog("[Init] ISR demo ready — health will deplete to 0 over ~10 frames.");
}

static void OnShutdown(Context* ctx) {
    (void)ctx;
    EngineLog("[Shutdown] ISR demo complete");
    Engine::Plugin::UnloadPlugins();
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    AppConfig config = {};
    config.title                   = "Engine ISR Demo";
    config.window_width            = 800;
    config.window_height           = 600;
    config.OnInit                  = OnInit;
    config.OnShutdown              = OnShutdown;
    config.OnLog                   = AppLog;
    config.global_memory_pool_size = 256 * 1024;
    config.thread_count            = 1;

    const EngineError err = EngineRun(&config);
    if (err != SUCCESS) {
        std::fprintf(stderr, "[Main] Engine failed to start with error code: %d\n", (int)err);
        return 1;
    }

    return 0;
}
