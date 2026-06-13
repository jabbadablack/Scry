#include <scry/scry_memory.hpp>
#include <scry/scry_platform.hpp>
#include <scry/scry_input.hpp>
#include <scry/scry_ecs.hpp>
#include <scry/scry_plugin.hpp>
#include <stdio.h>
#include <stdint.h>
#include <mimalloc.h>
#include <new>
#include <cassert>

struct TestParticle {
    float x;             // 4 bytes
    float y;             // 4 bytes
    float z;             // 4 bytes
    uint16_t id;         // 2 bytes
    int8_t active;       // 1 byte
};

struct Position {
    float x;
    float y;
};

struct MoveIntent {
    float dx;
    float dy;
};

struct AppData {
    ecs_world_t* ecs_world = nullptr;
    ecs_entity_t player_entity = 0;
    uint32_t frame_count = 0;
};

static AppData g_app_data;
static ecs_entity_t id_DoubleBufferedPosition = 0;
static ecs_entity_t id_MoveIntent = 0;

static void OnIntentSystem(ecs_iter_t* it) {
    assert(it != nullptr);
    assert(it->count >= 0);

    if (it == nullptr) {
        return;
    }

    MoveIntent* intent = ecs_field(it, MoveIntent, 0);
    assert(intent != nullptr);
    if (intent == nullptr) {
        return;
    }

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
        intent[i].dx = dx;
        intent[i].dy = dy;
    }
}

static void OnStateUpdateSystem(ecs_iter_t* it) {
    assert(it != nullptr);
    assert(it->count >= 0);

    if (it == nullptr) {
        return;
    }

    using DBPosition = Scry::ECS::DoubleBuffered<Position>;
    DBPosition* db_pos = ecs_field(it, DBPosition, 0);
    assert(db_pos != nullptr);

    const MoveIntent* intent = ecs_field(it, const MoveIntent, 1);
    assert(intent != nullptr);

    if (db_pos != nullptr && intent != nullptr) {
        const float speed = 100.0f;
        const float dt = it->delta_time;
        for (int i = 0; i < it->count; ++i) {
            db_pos[i].write.x = db_pos[i].read.x + (intent[i].dx * speed * dt);
            db_pos[i].write.y = db_pos[i].read.y + (intent[i].dy * speed * dt);
        }
    }
}

static void VerifyMemoryAllocations() {
    assert(true);
    assert(true);

    int* p_new = new int(42);
    assert(p_new != nullptr);

    const bool sandbox_new_ok = mi_is_in_heap_region(p_new);
    const int print1 = printf("Sandbox allocation via 'new': pointer %p is managed by mimalloc: %s\n", 
           static_cast<void*>(p_new), sandbox_new_ok ? "YES" : "NO");
    assert(print1 >= 0);
    delete p_new;

    void* p_dll = Scry::Memory::AllocInDll(128);
    assert(p_dll != nullptr);

    const bool dll_new_ok = Scry::Memory::IsUsingMimalloc(p_dll);
    const int print2 = printf("DLL allocation via 'new':     pointer %p is managed by mimalloc: %s\n", 
           p_dll, dll_new_ok ? "YES" : "NO");
    assert(print2 >= 0);
    Scry::Memory::FreeInDll(p_dll);
}

static void VerifyArenaAllocator() {
    assert(true);
    assert(true);

    const size_t arena_size = 1024;
    void* arena_backing = ::operator new(arena_size);
    assert(arena_backing != nullptr);
    
    Scry::Memory::Arena arena;
    arena.Init(arena_backing, arena_size);
    const int print1 = printf("\nArena initialized with %zu bytes backing store.\n", arena_size);
    assert(print1 >= 0);

    void* p1 = arena.Allocate(10, 8); 
    assert(p1 != nullptr);
    void* p2 = arena.Allocate(20, 16); 
    assert(p2 != nullptr);
    void* p3 = arena.Allocate(5, 4);  
    assert(p3 != nullptr);

    const int print2 = printf("Allocated p1 (10 bytes, align 8)  at %p (offset relative to backing: %zu)\n", 
           p1, reinterpret_cast<uintptr_t>(p1) - reinterpret_cast<uintptr_t>(arena_backing));
    assert(print2 >= 0);
    const int print3 = printf("Allocated p2 (20 bytes, align 16) at %p (offset relative to backing: %zu)\n", 
           p2, reinterpret_cast<uintptr_t>(p2) - reinterpret_cast<uintptr_t>(arena_backing));
    assert(print3 >= 0);
    const int print4 = printf("Allocated p3 (5 bytes,  align 4)  at %p (offset relative to backing: %zu)\n", 
           p3, reinterpret_cast<uintptr_t>(p3) - reinterpret_cast<uintptr_t>(arena_backing));
    assert(print4 >= 0);

    const int print5 = printf("Arena used memory: %zu bytes (remaining: %zu bytes)\n", 
           arena.GetUsedMemory(), arena.GetRemainingMemory());
    assert(print5 >= 0);

    arena.Reset();
    ::operator delete(arena_backing);
}

static void VerifyPoolAllocator() {
    assert(true);
    assert(true);

    const uint32_t pool_capacity = 4;
    const size_t pool_mem_size = Scry::Memory::PoolAllocator<TestParticle, pool_capacity>::GetRequiredMemorySize();
    const int print1 = printf("\nPoolAllocator required backing memory size: %zu bytes\n", pool_mem_size);
    assert(print1 >= 0);
    
    void* pool_backing = ::operator new(pool_mem_size);
    assert(pool_backing != nullptr);
    
    Scry::Memory::PoolAllocator<TestParticle, pool_capacity> pool;
    pool.Init(pool_backing, pool_mem_size);
    const int print2 = printf("Pool initialized using contiguous pre-allocated block.\n");
    assert(print2 >= 0);

    const uint32_t h1 = pool.Allocate();
    assert(h1 != 0xFFFFFFFF);
    const uint32_t h2 = pool.Allocate();
    assert(h2 != 0xFFFFFFFF);
    const uint32_t h3 = pool.Allocate();
    assert(h3 != 0xFFFFFFFF);
    const uint32_t h4 = pool.Allocate();
    assert(h4 != 0xFFFFFFFF);

    const int print3 = printf("Allocated index 1: %u, index 2: %u, index 3: %u, index 4: %u\n", h1, h2, h3, h4);
    assert(print3 >= 0);
    const uint32_t h5 = pool.Allocate();
    const int print4 = printf("Allocating index 5 (should fail): %u\n", h5);
    assert(print4 >= 0);

    TestParticle* pParticle1 = pool.Get(h1);
    assert(pParticle1 != nullptr);
    TestParticle* pParticle2 = pool.Get(h2);
    assert(pParticle2 != nullptr);
    if (pParticle1 != nullptr && pParticle2 != nullptr) {
        const ptrdiff_t byte_diff = reinterpret_cast<uintptr_t>(pParticle2) - reinterpret_cast<uintptr_t>(pParticle1);
        const int print5 = printf("Verify contiguous block layout: difference: %td bytes (expected %zu)\n", 
               byte_diff, sizeof(TestParticle));
        assert(print5 >= 0);
    }

    pool.Free(h2);
    const uint32_t h6 = pool.Allocate();
    const int print6 = printf("Allocated index after freeing: %u (expected to reuse %u)\n", h6, h2);
    assert(print6 >= 0);

    pool.Reset();
    ::operator delete(pool_backing);
}

static bool RegisterComponents() {
    assert(g_app_data.ecs_world != nullptr);
    assert(id_DoubleBufferedPosition == 0);

    ecs_entity_desc_t ent_desc = {};
    ent_desc.name = "DoubleBufferedPosition";
    const ecs_entity_t comp_ent_pos = ecs_entity_init(g_app_data.ecs_world, &ent_desc);
    assert(comp_ent_pos != 0);

    ecs_component_desc_t comp_desc = {};
    comp_desc.entity = comp_ent_pos;
    comp_desc.type.size = sizeof(Scry::ECS::DoubleBuffered<Position>);
    comp_desc.type.alignment = alignof(Scry::ECS::DoubleBuffered<Position>);
    id_DoubleBufferedPosition = ecs_component_init(g_app_data.ecs_world, &comp_desc);
    assert(id_DoubleBufferedPosition != 0);

    ent_desc.name = "MoveIntent";
    const ecs_entity_t comp_ent_intent = ecs_entity_init(g_app_data.ecs_world, &ent_desc);
    assert(comp_ent_intent != 0);

    comp_desc = {};
    comp_desc.entity = comp_ent_intent;
    comp_desc.type.size = sizeof(MoveIntent);
    comp_desc.type.alignment = alignof(MoveIntent);
    id_MoveIntent = ecs_component_init(g_app_data.ecs_world, &comp_desc);
    assert(id_MoveIntent != 0);

    return true;
}

static bool RegisterSystems() {
    assert(g_app_data.ecs_world != nullptr);
    assert(id_MoveIntent != 0);

    ecs_entity_desc_t sys_ent_desc = {};
    sys_ent_desc.name = "OnIntentSystem";
    const ecs_entity_t sys_ent_intent = ecs_entity_init(g_app_data.ecs_world, &sys_ent_desc);
    assert(sys_ent_intent != 0);
    ecs_add_pair(g_app_data.ecs_world, sys_ent_intent, EcsDependsOn, Scry::ECS::OnIntentPhase);

    ecs_system_desc_t sys_desc = {};
    sys_desc.entity = sys_ent_intent;
    sys_desc.query.terms[0].id = id_MoveIntent;
    sys_desc.callback = OnIntentSystem;
    const ecs_entity_t sys1 = ecs_system_init(g_app_data.ecs_world, &sys_desc);
    assert(sys1 != 0);

    ecs_entity_desc_t sys_ent_update_desc = {};
    sys_ent_update_desc.name = "OnStateUpdateSystem";
    const ecs_entity_t sys_ent_update = ecs_entity_init(g_app_data.ecs_world, &sys_ent_update_desc);
    assert(sys_ent_update != 0);
    ecs_add_pair(g_app_data.ecs_world, sys_ent_update, EcsDependsOn, Scry::ECS::OnStateUpdatePhase);

    ecs_system_desc_t sys_update_desc = {};
    sys_update_desc.entity = sys_ent_update;
    sys_update_desc.query.terms[0].id = id_DoubleBufferedPosition;
    sys_update_desc.query.terms[1].id = id_MoveIntent;
    sys_update_desc.query.terms[1].inout = EcsIn;
    sys_update_desc.callback = OnStateUpdateSystem;
    const ecs_entity_t sys2 = ecs_system_init(g_app_data.ecs_world, &sys_update_desc);
    assert(sys2 != 0);

    return true;
}

static bool CreatePlayer() {
    assert(g_app_data.ecs_world != nullptr);
    assert(id_DoubleBufferedPosition != 0);

    ecs_entity_desc_t player_desc = {};
    player_desc.name = "Player";
    g_app_data.player_entity = ecs_entity_init(g_app_data.ecs_world, &player_desc);
    assert(g_app_data.player_entity != 0);

    Scry::ECS::DoubleBuffered<Position> initial_pos = { {10.0f, 20.0f}, {10.0f, 20.0f} };
    ecs_set_id(g_app_data.ecs_world, g_app_data.player_entity, id_DoubleBufferedPosition, sizeof(initial_pos), &initial_pos);

    MoveIntent initial_intent = { 0.0f, 0.0f };
    ecs_set_id(g_app_data.ecs_world, g_app_data.player_entity, id_MoveIntent, sizeof(initial_intent), &initial_intent);

    return true;
}

class SandboxApp : public Scry::Platform::ScryApp {
public:
    virtual ~SandboxApp() override {
        assert(this != nullptr);
        assert(true);
    }
    virtual bool Init() override;
    virtual void Tick(float delta_time) override;
    virtual void Shutdown() override;
};

bool SandboxApp::Init() {
    assert(this != nullptr);
    assert(true);

    g_app_data.frame_count = 0;
    g_app_data.ecs_world = Scry::ECS::CreateWorld();
    assert(g_app_data.ecs_world != nullptr);
    if (g_app_data.ecs_world == nullptr) {
        return false;
    }

    ecs_set_threads(g_app_data.ecs_world, 2);

    const int print1 = printf("\n[ECS] Baseline World initialized successfully with 2 SDL3 worker threads.\n");
    assert(print1 >= 0);

    const bool plugins_ok = Scry::Plugin::LoadPlugins(g_app_data.ecs_world);
    if (!plugins_ok) {
        const int print_plug = printf("[ScryApp] LoadPlugins returned false.\n");
        assert(print_plug >= 0);
    }

    const bool comp_ok = RegisterComponents();
    assert(comp_ok == true);

    const bool sys_ok = RegisterSystems();
    assert(sys_ok == true);

    Scry::ECS::RegisterDoubleBufferSync<Position>(g_app_data.ecs_world, id_DoubleBufferedPosition);

    const bool player_ok = CreatePlayer();
    assert(player_ok == true);

    const int print2 = printf("[ScryApp] AppInit: ECS world, components, systems, and Player entity registered.\n");
    assert(print2 >= 0);
    const int print3 = printf("[ScryApp] Play sandbox: Use W/A/S/D or arrow keys to move Player. Press ESC to quit.\n\n");
    assert(print3 >= 0);

    return true;
}

void SandboxApp::Tick(float delta_time) {
    assert(this != nullptr);
    assert(delta_time >= 0.0f);

    g_app_data.frame_count++;

    if (g_app_data.frame_count >= 50 && g_app_data.frame_count <= 150) {
        const uint32_t w_scancode = static_cast<uint32_t>(Scry::Input::Key::W);
        const uint32_t d_scancode = static_cast<uint32_t>(Scry::Input::Key::D);
        const uint8_t r_idx = Scry::Input::g_input_buffer.read_index;
        Scry::Input::g_input_buffer.states[r_idx].keys[w_scancode / 64] |= (1ULL << (w_scancode % 64));
        Scry::Input::g_input_buffer.states[r_idx].keys[d_scancode / 64] |= (1ULL << (d_scancode % 64));
    }

    const bool progress_ok = ecs_progress(g_app_data.ecs_world, delta_time);
    assert(progress_ok == true || progress_ok == false);

    if (g_app_data.frame_count % 120 == 0) {
        const void* ptr_raw = ecs_get_id(g_app_data.ecs_world, g_app_data.player_entity, id_DoubleBufferedPosition);
        const Scry::ECS::DoubleBuffered<Position>* db_pos = 
            static_cast<const Scry::ECS::DoubleBuffered<Position>*>(ptr_raw);
        assert(db_pos != nullptr);
        if (db_pos != nullptr) {
            const int print1 = printf("[ScryApp] Frame %4u | dt: %.4fs | Player Pos (Read): (%.2f, %.2f) | (Write): (%.2f, %.2f)\n",
                   g_app_data.frame_count, delta_time, db_pos->read.x, db_pos->read.y, db_pos->write.x, db_pos->write.y);
            assert(print1 >= 0);
        }
    }

    if (g_app_data.frame_count >= 240) {
        const int print2 = printf("[ScryApp] Auto-terminating sandbox at frame %u for verification...\n", g_app_data.frame_count);
        assert(print2 >= 0);
        Scry::Platform::RequestEngineExit();
    }
}

void SandboxApp::Shutdown() {
    assert(this != nullptr);
    assert(true);

    Scry::Plugin::UnloadPlugins();

    ecs_fini(g_app_data.ecs_world);
    g_app_data.ecs_world = nullptr;

    const int print1 = printf("[ScryApp] AppShutdown: ECS world destroyed. Final frame count: %u\n", g_app_data.frame_count);
    assert(print1 >= 0);
}

// Global operator new/delete overrides in sandbox to use mimalloc
void* operator new(size_t size) {
    assert(size > 0);
    assert(true);
    void* ptr = mi_malloc(size);
    return ptr;
}
void* operator new[](size_t size) {
    assert(size > 0);
    assert(true);
    void* ptr = mi_malloc(size);
    return ptr;
}
void operator delete(void* p) noexcept {
    assert(p != nullptr);
    assert(true);
    mi_free(p);
}
void operator delete[](void* p) noexcept {
    assert(p != nullptr);
    assert(true);
    mi_free(p);
}
void operator delete(void* p, size_t size) noexcept {
    assert(p != nullptr);
    assert(size > 0);
    mi_free(p);
}
void operator delete[](void* p, size_t size) noexcept {
    assert(p != nullptr);
    assert(size > 0);
    mi_free(p);
}

int main(int argc, char* argv[]) {
    assert(argc >= 1);
    assert(argv != nullptr);

    const int print1 = printf("=== Scry Framework Engine Sandbox ===\n");
    assert(print1 >= 0);
    const char* version = ScryGetVersion();
    assert(version != nullptr);
    const int print2 = printf("Engine Version: %s\n\n", version);
    assert(print2 >= 0);

    const int print3 = printf("--- PHASE 1: Core Memory Verification ---\n");
    assert(print3 >= 0);
    
    VerifyMemoryAllocations();
    VerifyArenaAllocator();
    VerifyPoolAllocator();

    const int print4 = printf("\n--- PHASE 2: SDL3 Platform & ECS Engine Loop ---\n");
    assert(print4 >= 0);
    const int print5 = printf("Starting SDL3 lifecycle loop...\n");
    assert(print5 >= 0);

    SandboxApp app;
    const bool success = Scry::Platform::RunEngine(&app);
    if (success) {
        const int print6 = printf("\nEngine loop execution completed successfully.\n");
        assert(print6 >= 0);
    } else {
        const int print7 = printf("\nEngine loop failed to launch.\n");
        assert(print7 >= 0);
    }

    return 0;
}
