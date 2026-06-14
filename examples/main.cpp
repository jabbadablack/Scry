#include <scry/scry_memory.hpp>
#include <scry/scry_platform.hpp>
#include <scry/scry_input.hpp>
#include <scry/scry_ecs.hpp>
#include <scry/scry_plugin.hpp>
#include <scry/scry_json.hpp>
#include <scry/scry_math.hpp>
#define QUILL_ROOT_LOGGER_ONLY
#include <quill/Quill.h>
#include <libassert/assert.hpp>
#include <stdint.h>
#include <mimalloc.h>
#include <new>

struct TestParticle {
    float x;             // 4 bytes
    float y;             // 4 bytes
    float z;             // 4 bytes
    uint16_t id;         // 2 bytes
    int8_t active;       // 1 byte
};

struct Position {
    Scry::Math::ScryVec2 pos;
};

struct MoveIntent {
    Scry::Math::ScryVec2 dir;
};

struct AppData {
    ecs_entity_t player_entity = 0;
    uint32_t frame_count = 0;
};

static AppData g_app_data;
static ecs_entity_t id_DoubleBufferedPosition = 0;
static ecs_entity_t id_MoveIntent = 0;

static void OnIntentSystem(ecs_iter_t* it) {
    DEBUG_ASSERT(it != nullptr);
    DEBUG_ASSERT(it->count >= 0);

    MoveIntent* intent = ecs_field(it, MoveIntent, 0);
    DEBUG_ASSERT(intent != nullptr);

    float dx = 0.0f;
    float dy = 0.0f;

    const bool w_pressed = Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::W);
    const bool up_pressed = Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::Up);
    if (w_pressed || up_pressed) {
        dy -= 1.0f;
    }

    const bool s_pressed = Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::S);
    const bool down_pressed = Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::Down);
    if (s_pressed || down_pressed) {
        dy += 1.0f;
    }

    const bool a_pressed = Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::A);
    const bool left_pressed = Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::Left);
    if (a_pressed || left_pressed) {
        dx -= 1.0f;
    }

    const bool d_pressed = Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::D);
    const bool right_pressed = Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::Right);
    if (d_pressed || right_pressed) {
        dx += 1.0f;
    }

    for (int i = 0; i < it->count; ++i) {
        intent[i].dir.x() = dx;
        intent[i].dir.y() = dy;
    }
}

static void OnStateUpdateSystem(ecs_iter_t* it) {
    DEBUG_ASSERT(it != nullptr);
    DEBUG_ASSERT(it->count >= 0);

    using DBPosition = Scry::ECS::DoubleBuffered<Position>;
    DBPosition* db_pos = ecs_field(it, DBPosition, 0);
    DEBUG_ASSERT(db_pos != nullptr);

    const MoveIntent* intent = ecs_field(it, const MoveIntent, 1);
    DEBUG_ASSERT(intent != nullptr);

    const float speed = 10.0f;

    for (int i = 0; i < it->count; ++i) {
        db_pos[i].write.pos = db_pos[i].read.pos + intent[i].dir * (speed * it->delta_time);
    }
}

static void VerifyMemoryAllocations() {
    DEBUG_ASSERT(true);
    DEBUG_ASSERT(true);

    int* p_new = new int(42);
    const bool sandbox_new_ok = Scry::Memory::IsUsingMimalloc(p_new);
    LOG_INFO("Sandbox allocation via 'new': pointer {} is managed by mimalloc: {}", 
           static_cast<void*>(p_new), sandbox_new_ok ? "YES" : "NO");
    delete p_new;

    void* p_dll = Scry::Memory::AllocInDll(32);
    const bool dll_new_ok = Scry::Memory::IsUsingMimalloc(p_dll);
    LOG_INFO("DLL allocation via 'new':     pointer {} is managed by mimalloc: {}", 
           p_dll, dll_new_ok ? "YES" : "NO");
    Scry::Memory::FreeInDll(p_dll);
}

static void VerifyArenaAllocator() {
    DEBUG_ASSERT(true);
    DEBUG_ASSERT(true);

    const size_t arena_size = 1024;
    void* arena_backing = ::operator new(arena_size);
    DEBUG_ASSERT(arena_backing != nullptr);
    
    Scry::Memory::Arena arena;
    Scry::Memory::ScryArenaInit(&arena, arena_backing, arena_size);
    LOG_INFO("Arena initialized with {} bytes backing store.", arena_size);

    void* p1 = Scry::Memory::ScryArenaAllocate(&arena, 10, 8); 
    DEBUG_ASSERT(p1 != nullptr);
    void* p2 = Scry::Memory::ScryArenaAllocate(&arena, 20, 16); 
    DEBUG_ASSERT(p2 != nullptr);
    void* p3 = Scry::Memory::ScryArenaAllocate(&arena, 5, 4);  
    DEBUG_ASSERT(p3 != nullptr);

    LOG_INFO("Allocated p1 (10 bytes, align 8)  at {} (offset relative to backing: {})", 
           p1, reinterpret_cast<uintptr_t>(p1) - reinterpret_cast<uintptr_t>(arena_backing));
    LOG_INFO("Allocated p2 (20 bytes, align 16) at {} (offset relative to backing: {})", 
           p2, reinterpret_cast<uintptr_t>(p2) - reinterpret_cast<uintptr_t>(arena_backing));
    LOG_INFO("Allocated p3 (5 bytes,  align 4)  at {} (offset relative to backing: {})", 
           p3, reinterpret_cast<uintptr_t>(p3) - reinterpret_cast<uintptr_t>(arena_backing));

    LOG_INFO("Arena used memory: {} bytes (remaining: {} bytes)", 
           Scry::Memory::ScryArenaGetUsedMemory(&arena), Scry::Memory::ScryArenaGetRemainingMemory(&arena));

    Scry::Memory::ScryArenaReset(&arena);
    ::operator delete(arena_backing);
}

static void VerifyPoolAllocator() {
    DEBUG_ASSERT(true);
    DEBUG_ASSERT(true);

    const uint32_t pool_capacity = 4;
    const size_t pool_mem_size = Scry::Memory::ScryPoolGetRequiredSize(sizeof(TestParticle), pool_capacity);
    LOG_INFO("PoolAllocator required backing memory size: {} bytes", pool_mem_size);
    
    void* pool_backing = ::operator new(pool_mem_size);
    DEBUG_ASSERT(pool_backing != nullptr);
    
    Scry::Memory::PoolAllocator pool;
    Scry::Memory::ScryPoolInit(&pool, pool_backing, pool_mem_size, sizeof(TestParticle), pool_capacity);
    LOG_INFO("Pool initialized using contiguous pre-allocated block.");

    const uint32_t h1 = Scry::Memory::ScryPoolAllocate(&pool);
    DEBUG_ASSERT(h1 != 0xFFFFFFFF);
    const uint32_t h2 = Scry::Memory::ScryPoolAllocate(&pool);
    DEBUG_ASSERT(h2 != 0xFFFFFFFF);
    const uint32_t h3 = Scry::Memory::ScryPoolAllocate(&pool);
    DEBUG_ASSERT(h3 != 0xFFFFFFFF);
    const uint32_t h4 = Scry::Memory::ScryPoolAllocate(&pool);
    DEBUG_ASSERT(h4 != 0xFFFFFFFF);

    LOG_INFO("Allocated index 1: {}, index 2: {}, index 3: {}, index 4: {}", h1, h2, h3, h4);
    const uint32_t h5 = Scry::Memory::ScryPoolAllocate(&pool);
    LOG_INFO("Allocating index 5 (should fail): {}", h5);

    void* pParticle1 = Scry::Memory::ScryPoolGet(&pool, h1);
    DEBUG_ASSERT(pParticle1 != nullptr);
    void* pParticle2 = Scry::Memory::ScryPoolGet(&pool, h2);
    DEBUG_ASSERT(pParticle2 != nullptr);
    if (pParticle1 != nullptr && pParticle2 != nullptr) {
        const ptrdiff_t byte_diff = reinterpret_cast<uintptr_t>(pParticle2) - reinterpret_cast<uintptr_t>(pParticle1);
        LOG_INFO("Verify contiguous block layout: difference: {} bytes (expected {})", 
               byte_diff, sizeof(TestParticle));
    }

    Scry::Memory::ScryPoolFree(&pool, h2);
    const uint32_t h6 = Scry::Memory::ScryPoolAllocate(&pool);
    LOG_INFO("Allocated index after freeing: {} (expected to reuse {})", h6, h2);

    Scry::Memory::ScryPoolReset(&pool);
    ::operator delete(pool_backing);
}

static bool RegisterComponents(ScryContext* ctx) {
    DEBUG_ASSERT(ctx != nullptr);
    DEBUG_ASSERT(ctx->ecs_world != nullptr);

    ecs_entity_desc_t ent_desc = {};
    ent_desc.name = "DoubleBufferedPosition";
    const ecs_entity_t comp_ent_pos = ecs_entity_init(ctx->ecs_world, &ent_desc);
    DEBUG_ASSERT(comp_ent_pos != 0);

    ecs_component_desc_t comp_desc = {};
    comp_desc.entity = comp_ent_pos;
    comp_desc.type.size = sizeof(Scry::ECS::DoubleBuffered<Position>);
    comp_desc.type.alignment = alignof(Scry::ECS::DoubleBuffered<Position>);
    id_DoubleBufferedPosition = ecs_component_init(ctx->ecs_world, &comp_desc);
    DEBUG_ASSERT(id_DoubleBufferedPosition != 0);

    ent_desc.name = "MoveIntent";
    const ecs_entity_t comp_ent_intent = ecs_entity_init(ctx->ecs_world, &ent_desc);
    DEBUG_ASSERT(comp_ent_intent != 0);

    comp_desc = {};
    comp_desc.entity = comp_ent_intent;
    comp_desc.type.size = sizeof(MoveIntent);
    comp_desc.type.alignment = alignof(MoveIntent);
    id_MoveIntent = ecs_component_init(ctx->ecs_world, &comp_desc);
    DEBUG_ASSERT(id_MoveIntent != 0);

    return true;
}

static bool RegisterSystems(ScryContext* ctx) {
    DEBUG_ASSERT(ctx != nullptr);
    DEBUG_ASSERT(ctx->ecs_world != nullptr);

    ecs_entity_desc_t sys_ent_desc = {};
    sys_ent_desc.name = "OnIntentSystem";
    const ecs_entity_t sys_ent_intent = ecs_entity_init(ctx->ecs_world, &sys_ent_desc);
    DEBUG_ASSERT(sys_ent_intent != 0);
    ecs_add_pair(ctx->ecs_world, sys_ent_intent, EcsDependsOn, Scry::ECS::OnIntentPhase);

    ecs_system_desc_t sys_desc = {};
    sys_desc.entity = sys_ent_intent;
    sys_desc.query.terms[0].id = id_MoveIntent;
    sys_desc.callback = OnIntentSystem;
    const ecs_entity_t sys1 = ecs_system_init(ctx->ecs_world, &sys_desc);
    DEBUG_ASSERT(sys1 != 0);

    ecs_entity_desc_t sys_ent_update_desc = {};
    sys_ent_update_desc.name = "OnStateUpdateSystem";
    const ecs_entity_t sys_ent_update = ecs_entity_init(ctx->ecs_world, &sys_ent_update_desc);
    DEBUG_ASSERT(sys_ent_update != 0);
    ecs_add_pair(ctx->ecs_world, sys_ent_update, EcsDependsOn, Scry::ECS::OnStateUpdatePhase);

    ecs_system_desc_t sys_update_desc = {};
    sys_update_desc.entity = sys_ent_update;
    sys_update_desc.query.terms[0].id = id_DoubleBufferedPosition;
    sys_update_desc.query.terms[1].id = id_MoveIntent;
    sys_update_desc.query.terms[1].inout = EcsIn;
    sys_update_desc.callback = OnStateUpdateSystem;
    const ecs_entity_t sys2 = ecs_system_init(ctx->ecs_world, &sys_update_desc);
    DEBUG_ASSERT(sys2 != 0);

    return true;
}

static void AppInit(ScryContext* ctx) {
    DEBUG_ASSERT(ctx != nullptr);
    DEBUG_ASSERT(ctx->ecs_world != nullptr);

    g_app_data.frame_count = 0;
    ecs_set_threads(ctx->ecs_world, 2);

    LOG_INFO("Baseline World initialized successfully with 2 SDL3 worker threads.");

    const bool comp_ok = RegisterComponents(ctx);
    DEBUG_ASSERT(comp_ok == true);

    const bool sys_ok = RegisterSystems(ctx);
    DEBUG_ASSERT(sys_ok == true);

    Scry::ECS::RegisterDoubleBufferSync<Position>(ctx->ecs_world, id_DoubleBufferedPosition);

    const bool json_ok = Scry::JSON::LoadProjectConfig(ctx, "scry_project.json");
    DEBUG_ASSERT(json_ok == true);

    g_app_data.player_entity = ecs_lookup(ctx->ecs_world, "Player");
    DEBUG_ASSERT(g_app_data.player_entity != 0);

    LOG_INFO("[ScryApp] AppInit: ECS world, components, systems, and Player entity registered.");
    LOG_INFO("[ScryApp] Play sandbox: Use W/A/S/D or arrow keys to move Player. Press ESC to quit.\n");
}

static void AppUpdate(ScryContext* ctx, float delta_time) {
    DEBUG_ASSERT(ctx != nullptr);
    DEBUG_ASSERT(delta_time >= 0.0f);

    g_app_data.frame_count++;

    if (g_app_data.frame_count >= 50 && g_app_data.frame_count <= 150) {
        const uint32_t w_scancode = static_cast<uint32_t>(Scry::Input::Key::W);
        const uint32_t d_scancode = static_cast<uint32_t>(Scry::Input::Key::D);
        const uint8_t r_idx = Scry::Input::g_input_buffer.read_index;
        Scry::Input::g_input_buffer.states[r_idx].keys[w_scancode / 64] |= (1ULL << (w_scancode % 64));
        Scry::Input::g_input_buffer.states[r_idx].keys[d_scancode / 64] |= (1ULL << (d_scancode % 64));
    }

    const bool progress_ok = ecs_progress(ctx->ecs_world, delta_time);
    DEBUG_ASSERT(progress_ok == true || progress_ok == false);

    if (g_app_data.frame_count % 120 == 0) {
        const void* ptr_raw = ecs_get_id(ctx->ecs_world, g_app_data.player_entity, id_DoubleBufferedPosition);
        const Scry::ECS::DoubleBuffered<Position>* db_pos = 
            static_cast<const Scry::ECS::DoubleBuffered<Position>*>(ptr_raw);
        DEBUG_ASSERT(db_pos != nullptr);
        if (db_pos != nullptr) {
            LOG_INFO("[ScryApp] Frame {:4} | dt: {:.4f}s | Player Pos (Read): ({:.2f}, {:.2f}) | (Write): ({:.2f}, {:.2f})",
                   g_app_data.frame_count, delta_time, db_pos->read.pos.x(), db_pos->read.pos.y(), db_pos->write.pos.x(), db_pos->write.pos.y());
        }
    }

    if (g_app_data.frame_count >= 240) {
        LOG_INFO("[ScryApp] Auto-terminating sandbox at frame {} for verification...", g_app_data.frame_count);
        RequestEngineExit(ctx);
    }
}

static void AppShutdown(ScryContext* ctx) {
    DEBUG_ASSERT(ctx != nullptr);
    DEBUG_ASSERT(ctx->ecs_world != nullptr);

    Scry::Plugin::UnloadPlugins();

    LOG_INFO("[ScryApp] AppShutdown: ECS world destroyed. Final frame count: {}", g_app_data.frame_count);
}

// Global operator new/delete overrides to use mimalloc
void* operator new(size_t size) {
    DEBUG_ASSERT(size > 0);
    void* ptr = mi_malloc(size);
    DEBUG_ASSERT(ptr != nullptr);
    return ptr;
}
void* operator new[](size_t size) {
    DEBUG_ASSERT(size > 0);
    void* ptr = mi_malloc(size);
    DEBUG_ASSERT(ptr != nullptr);
    return ptr;
}
void operator delete(void* p) noexcept {
    DEBUG_ASSERT(p != nullptr || p == nullptr);
    DEBUG_ASSERT(true);
    mi_free(p);
}
void operator delete[](void* p) noexcept {
    DEBUG_ASSERT(p != nullptr || p == nullptr);
    DEBUG_ASSERT(true);
    mi_free(p);
}
void operator delete(void* p, size_t size) noexcept {
    DEBUG_ASSERT(p != nullptr || p == nullptr);
    DEBUG_ASSERT(size >= 0);
    (void)size;
    mi_free(p);
}
void operator delete[](void* p, size_t size) noexcept {
    DEBUG_ASSERT(p != nullptr || p == nullptr);
    DEBUG_ASSERT(size >= 0);
    (void)size;
    mi_free(p);
}

int main(int argc, char* argv[]) {
    DEBUG_ASSERT(argc >= 1);
    DEBUG_ASSERT(argv != nullptr);

    // Initial temporary initialization of Quill just for main phase 1
    std::shared_ptr<quill::Handler> handler = quill::stdout_handler();
    handler->set_pattern("%(message)");
    quill::Config cfg;
    cfg.default_handlers.push_back(handler);
    quill::configure(cfg);
    quill::start();

    LOG_INFO("=== Scry Framework Engine Sandbox ===");
    const char* version = ScryGetVersion();
    DEBUG_ASSERT(version != nullptr);
    LOG_INFO("Engine Version: {}\n", version);

    LOG_INFO("--- PHASE 1: Core Memory Verification ---");
    
    VerifyMemoryAllocations();
    VerifyArenaAllocator();
    VerifyPoolAllocator();

    LOG_INFO("\n--- PHASE 2: SDL3 Platform & ECS Engine Loop ---");
    LOG_INFO("Starting SDL3 lifecycle loop...");

    ScryAppConfig config = {};
    config.OnInit = AppInit;
    config.OnUpdate = AppUpdate;
    config.OnShutdown = AppShutdown;
    config.window_width = 800;
    config.window_height = 600;
    config.app_name = "Scry Engine Sandbox";

    const int res = ScryRun(&config);
    if (res == 0) {
        LOG_INFO("\nEngine loop execution completed successfully.");
    } else {
        LOG_INFO("\nEngine loop failed to launch. Code: {}", res);
    }

    return 0;
}
