#include <scry/scry_memory.hpp>
#include <scry/scry_platform.hpp>
#include <scry/scry_input.hpp>
#include <scry/scry_ecs.hpp>
#include <stdio.h>
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

// ECS Components
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

static ecs_entity_t id_DoubleBufferedPosition = 0;
static ecs_entity_t id_MoveIntent = 0;

// OnIntentSystem: Read from input double-buffer, write movement intent
static void OnIntentSystem(ecs_iter_t* it) {
    MoveIntent* intent = ecs_field(it, MoveIntent, 0);
    
    float dx = 0.0f;
    float dy = 0.0f;
    
    if (Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::W) || 
        Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::Up)) {
        dy -= 1.0f;
    }
    if (Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::S) || 
        Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::Down)) {
        dy += 1.0f;
    }
    if (Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::A) || 
        Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::Left)) {
        dx -= 1.0f;
    }
    if (Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::D) || 
        Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::Right)) {
        dx += 1.0f;
    }

    for (int i = 0; i < it->count; ++i) {
        intent[i].dx = dx;
        intent[i].dy = dy;
    }
}

// OnStateUpdateSystem: Read MoveIntent and previous state (read), write new state (write)
static void OnStateUpdateSystem(ecs_iter_t* it) {
    using DBPosition = Scry::ECS::DoubleBuffered<Position>;
    DBPosition* db_pos = ecs_field(it, DBPosition, 0);
    const MoveIntent* intent = ecs_field(it, const MoveIntent, 1);

    const float speed = 100.0f; // 100 units/second
    float dt = it->delta_time;

    for (int i = 0; i < it->count; ++i) {
        db_pos[i].write.x = db_pos[i].read.x + (intent[i].dx * speed * dt);
        db_pos[i].write.y = db_pos[i].read.y + (intent[i].dy * speed * dt);
    }
}


// App Lifecycle callbacks
bool AppInit(void* user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    data->frame_count = 0;

    // Create ECS world with mimalloc and SDL3 overrides
    data->ecs_world = Scry::ECS::CreateWorld();
    if (!data->ecs_world) {
        return false;
    }

    // Set 2 worker threads to verify SDL3 threading and atomic hooks integration
    ecs_set_threads(data->ecs_world, 2);

    printf("\n[ECS] Baseline World initialized successfully with 2 SDL3 worker threads.\n");

    // Register components (C-structs for strict data locality)
    ecs_entity_desc_t ent_desc = {};
    
    ent_desc.name = "DoubleBufferedPosition";
    ecs_entity_t comp_ent_pos = ecs_entity_init(data->ecs_world, &ent_desc);

    ecs_component_desc_t comp_desc = {};
    comp_desc.entity = comp_ent_pos;
    comp_desc.type.size = sizeof(Scry::ECS::DoubleBuffered<Position>);
    comp_desc.type.alignment = alignof(Scry::ECS::DoubleBuffered<Position>);
    id_DoubleBufferedPosition = ecs_component_init(data->ecs_world, &comp_desc);

    ent_desc.name = "MoveIntent";
    ecs_entity_t comp_ent_intent = ecs_entity_init(data->ecs_world, &ent_desc);

    comp_desc = {};
    comp_desc.entity = comp_ent_intent;
    comp_desc.type.size = sizeof(MoveIntent);
    comp_desc.type.alignment = alignof(MoveIntent);
    id_MoveIntent = ecs_component_init(data->ecs_world, &comp_desc);

    // Register systems
    // 1. OnIntentSystem - runs in OnIntent phase
    ecs_entity_desc_t sys_ent_desc = {};
    sys_ent_desc.name = "OnIntentSystem";
    ecs_entity_t sys_ent_intent = ecs_entity_init(data->ecs_world, &sys_ent_desc);
    ecs_add_pair(data->ecs_world, sys_ent_intent, EcsDependsOn, Scry::ECS::OnIntentPhase);

    ecs_system_desc_t sys_desc = {};
    sys_desc.entity = sys_ent_intent;
    sys_desc.query.terms[0].id = id_MoveIntent;
    sys_desc.callback = OnIntentSystem;
    ecs_system_init(data->ecs_world, &sys_desc);

    // 2. OnStateUpdateSystem - runs in OnStateUpdate phase
    ecs_entity_desc_t sys_ent_update_desc = {};
    sys_ent_update_desc.name = "OnStateUpdateSystem";
    ecs_entity_t sys_ent_update = ecs_entity_init(data->ecs_world, &sys_ent_update_desc);
    ecs_add_pair(data->ecs_world, sys_ent_update, EcsDependsOn, Scry::ECS::OnStateUpdatePhase);

    ecs_system_desc_t sys_update_desc = {};
    sys_update_desc.entity = sys_ent_update;
    sys_update_desc.query.terms[0].id = id_DoubleBufferedPosition;
    sys_update_desc.query.terms[1].id = id_MoveIntent;
    sys_update_desc.query.terms[1].inout = EcsIn;
    sys_update_desc.callback = OnStateUpdateSystem;
    ecs_system_init(data->ecs_world, &sys_update_desc);


    // 3. Register our template DoubleBufferedSync system for Position - runs in OnReact phase
    Scry::ECS::RegisterDoubleBufferSync<Position>(data->ecs_world, id_DoubleBufferedPosition);

    // Create player entity
    ecs_entity_desc_t player_desc = {};
    player_desc.name = "Player";
    data->player_entity = ecs_entity_init(data->ecs_world, &player_desc);
    
    // Initialize components
    Scry::ECS::DoubleBuffered<Position> initial_pos = { {10.0f, 20.0f}, {10.0f, 20.0f} };
    ecs_set_id(data->ecs_world, data->player_entity, id_DoubleBufferedPosition, sizeof(initial_pos), &initial_pos);
    
    MoveIntent initial_intent = { 0.0f, 0.0f };
    ecs_set_id(data->ecs_world, data->player_entity, id_MoveIntent, sizeof(initial_intent), &initial_intent);

    printf("[ScryApp] AppInit: ECS world, components, systems, and Player entity registered.\n");
    printf("[ScryApp] Play sandbox: Use W/A/S/D or arrow keys to move Player. Press ESC to quit.\n\n");
    return true;
}


void AppTick(void* user_data, float delta_time) {
    AppData* data = static_cast<AppData*>(user_data);
    data->frame_count++;

    // Programmatically simulate pressing W and D keys between frames 50 and 150 to verify the pipeline
    if (data->frame_count >= 50 && data->frame_count <= 150) {
        uint32_t w_scancode = static_cast<uint32_t>(Scry::Input::Key::W);
        uint32_t d_scancode = static_cast<uint32_t>(Scry::Input::Key::D);
        uint8_t r_idx = Scry::Input::g_input_buffer.read_index;
        Scry::Input::g_input_buffer.states[r_idx].keys[w_scancode / 64] |= (1ULL << (w_scancode % 64));
        Scry::Input::g_input_buffer.states[r_idx].keys[d_scancode / 64] |= (1ULL << (d_scancode % 64));
    }

    // Progress ECS world (zero heap allocations)
    ecs_progress(data->ecs_world, delta_time);

    // Print Player Position every 120 frames
    if (data->frame_count % 120 == 0) {
        const Scry::ECS::DoubleBuffered<Position>* db_pos = 
            static_cast<const Scry::ECS::DoubleBuffered<Position>*>(
                ecs_get_id(data->ecs_world, data->player_entity, id_DoubleBufferedPosition)
            );
        if (db_pos) {
            printf("[ScryApp] Frame %4u | dt: %.4fs | Player Pos (Read): (%.2f, %.2f) | (Write): (%.2f, %.2f)\n",
                   data->frame_count, delta_time, db_pos->read.x, db_pos->read.y, db_pos->write.x, db_pos->write.y);
        }
    }

    // Auto-terminate sandbox after 240 frames
    if (data->frame_count >= 240) {
        printf("[ScryApp] Auto-terminating sandbox at frame %u for verification...\n", data->frame_count);
        Scry::Platform::RequestEngineExit();
    }
}

void AppShutdown(void* user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    if (data->ecs_world) {
        ecs_fini(data->ecs_world);
        data->ecs_world = nullptr;
    }
    printf("[ScryApp] AppShutdown: ECS world destroyed. Final frame count: %u\n", data->frame_count);
}

// Global operator new/delete overrides in sandbox to use mimalloc
void* operator new(size_t size) {
    return mi_malloc(size);
}
void* operator new[](size_t size) {
    return mi_malloc(size);
}
void operator delete(void* p) noexcept {
    mi_free(p);
}
void operator delete[](void* p) noexcept {
    mi_free(p);
}
void operator delete(void* p, size_t size) noexcept {
    mi_free(p);
}
void operator delete[](void* p, size_t size) noexcept {
    mi_free(p);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("=== Scry Framework Engine Sandbox ===\n");
    printf("Engine Version: %s\n\n", ScryGetVersion());

    // ----------------------------------------------------
    // Phase 1: Core Memory Verification
    // ----------------------------------------------------
    printf("--- PHASE 1: Core Memory Verification ---\n");
    
    int* p_new = new int(42);
    bool sandbox_new_ok = mi_is_in_heap_region(p_new);
    printf("Sandbox allocation via 'new': pointer %p is managed by mimalloc: %s\n", 
           (void*)p_new, sandbox_new_ok ? "YES" : "NO");
    delete p_new;

    void* p_dll = Scry::Memory::AllocInDll(128);
    bool dll_new_ok = Scry::Memory::IsUsingMimalloc(p_dll);
    printf("DLL allocation via 'new':     pointer %p is managed by mimalloc: %s\n", 
           p_dll, dll_new_ok ? "YES" : "NO");
    Scry::Memory::FreeInDll(p_dll);

    const size_t arena_size = 1024;
    void* arena_backing = ::operator new(arena_size);
    
    Scry::Memory::Arena arena;
    arena.Init(arena_backing, arena_size);
    printf("\nArena initialized with %zu bytes backing store.\n", arena_size);

    void* p1 = arena.Allocate(10, 8); 
    void* p2 = arena.Allocate(20, 16); 
    void* p3 = arena.Allocate(5, 4);  

    printf("Allocated p1 (10 bytes, align 8)  at %p (offset relative to backing: %zu)\n", 
           p1, (uintptr_t)p1 - (uintptr_t)arena_backing);
    printf("Allocated p2 (20 bytes, align 16) at %p (offset relative to backing: %zu)\n", 
           p2, (uintptr_t)p2 - (uintptr_t)arena_backing);
    printf("Allocated p3 (5 bytes,  align 4)  at %p (offset relative to backing: %zu)\n", 
           p3, (uintptr_t)p3 - (uintptr_t)arena_backing);

    printf("Arena used memory: %zu bytes (remaining: %zu bytes)\n", 
           arena.GetUsedMemory(), arena.GetRemainingMemory());
    arena.Reset();
    ::operator delete(arena_backing);

    const uint32_t pool_capacity = 4;
    size_t pool_mem_size = Scry::Memory::PoolAllocator<TestParticle, pool_capacity>::GetRequiredMemorySize();
    printf("\nPoolAllocator required backing memory size: %zu bytes\n", pool_mem_size);
    
    void* pool_backing = ::operator new(pool_mem_size);
    
    Scry::Memory::PoolAllocator<TestParticle, pool_capacity> pool;
    pool.Init(pool_backing, pool_mem_size);
    printf("Pool initialized using contiguous pre-allocated block.\n");

    uint32_t h1 = pool.Allocate();
    uint32_t h2 = pool.Allocate();
    uint32_t h3 = pool.Allocate();
    uint32_t h4 = pool.Allocate();

    printf("Allocated index 1: %u, index 2: %u, index 3: %u, index 4: %u\n", h1, h2, h3, h4);
    uint32_t h5 = pool.Allocate();
    printf("Allocating index 5 (should fail): %u\n", h5);

    TestParticle* pParticle1 = pool.Get(h1);
    TestParticle* pParticle2 = pool.Get(h2);
    if (pParticle1 && pParticle2) {
        ptrdiff_t byte_diff = (uintptr_t)pParticle2 - (uintptr_t)pParticle1;
        printf("Verify contiguous block layout: difference: %td bytes (expected %zu)\n", 
               byte_diff, sizeof(TestParticle));
    }

    pool.Free(h2);
    uint32_t h6 = pool.Allocate();
    printf("Allocated index after freeing: %u (expected to reuse %u)\n", h6, h2);

    pool.Reset();
    ::operator delete(pool_backing);

    // ----------------------------------------------------
    // Phase 2: Platform & ECS Engine Loop Verification
    // ----------------------------------------------------
    printf("\n--- PHASE 2: SDL3 Platform & ECS Engine Loop ---\n");
    printf("Starting SDL3 lifecycle loop...\n");

    AppData app_data;
    Scry::Platform::ScryApp app;
    app.Init = AppInit;
    app.Tick = AppTick;
    app.Shutdown = AppShutdown;
    app.user_data = &app_data;

    bool success = Scry::Platform::RunEngine(&app);
    if (success) {
        printf("\nEngine loop execution completed successfully.\n");
    } else {
        printf("\nEngine loop failed to launch.\n");
    }

    return 0;
}
