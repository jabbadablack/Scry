#include <scry/scry_memory.hpp>
#include <scry/scry_platform.hpp>
#include <scry/scry_input.hpp>
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

struct AppData {
    uint32_t frame_count = 0;
};

extern "C" SCRY_API const char* ScryGetVersion();

// App Lifecycle callbacks
bool AppInit(void* user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    data->frame_count = 0;
    printf("\n[ScryApp] Init: Application state reset. Press W, Space, or left/right mouse clicks to test inputs!\n");
    printf("[ScryApp] Press ESCAPE or close the window to exit the engine loop.\n\n");
    return true;
}

void AppTick(void* user_data, float delta_time) {
    AppData* data = static_cast<AppData*>(user_data);
    data->frame_count++;

    // Read input from the global double-buffered input bitmask
    bool w_down = Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::W);
    bool space_down = Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::Space);
    bool click_down = Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::MouseL);
    bool right_click = Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::MouseR);
    
    int16_t mouse_x = 0;
    int16_t mouse_y = 0;
    Scry::Input::g_input_buffer.GetMousePos(mouse_x, mouse_y);

    // Print status every 120 frames (~2 seconds with 1ms delay)
    if (data->frame_count % 120 == 0) {
        printf("[ScryApp] Frame %4u | dt: %.4fs | Keys: W=%d, Space=%d | Mouse: LClick=%d, RClick=%d | Pos: (%d, %d)\n",
               data->frame_count, delta_time, w_down, space_down, click_down, right_click, mouse_x, mouse_y);
    }

    // Auto-terminate sandbox after 240 frames (~4 seconds) for automated test execution
    if (data->frame_count >= 240) {
        printf("[ScryApp] Auto-terminating sandbox at frame %u for verification...\n", data->frame_count);
        Scry::Platform::RequestEngineExit();
    }
}


void AppShutdown(void* user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    printf("[ScryApp] Shutdown: Final frame count: %u\n", data->frame_count);
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
    
    // Test operator new in Sandbox
    int* p_new = new int(42);
    bool sandbox_new_ok = mi_is_in_heap_region(p_new);
    printf("Sandbox allocation via 'new': pointer %p is managed by mimalloc: %s\n", 
           (void*)p_new, sandbox_new_ok ? "YES" : "NO");
    delete p_new;

    // Test operator new in DLL
    void* p_dll = Scry::Memory::AllocInDll(128);
    bool dll_new_ok = Scry::Memory::IsUsingMimalloc(p_dll);
    printf("DLL allocation via 'new':     pointer %p is managed by mimalloc: %s\n", 
           p_dll, dll_new_ok ? "YES" : "NO");
    Scry::Memory::FreeInDll(p_dll);

    // Test Arena Allocator
    const size_t arena_size = 1024;
    void* arena_backing = ::operator new(arena_size);
    
    Scry::Memory::Arena arena;
    arena.Init(arena_backing, arena_size);
    printf("\nArena initialized with %zu bytes backing store.\n", arena_size);

    void* p1 = arena.Allocate(10, 8); // 8-byte aligned
    void* p2 = arena.Allocate(20, 16); // 16-byte aligned
    void* p3 = arena.Allocate(5, 4);  // 4-byte aligned

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

    // Test Pool Allocator
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
    // Phase 2: Platform & Engine Loop Verification
    // ----------------------------------------------------
    printf("\n--- PHASE 2: SDL3 Platform & Engine Loop ---\n");
    printf("Starting SDL3 lifecycle loop...\n");

    AppData app_data;
    Scry::Platform::ScryApp app;
    app.Init = AppInit;
    app.Tick = AppTick;
    app.Shutdown = AppShutdown;
    app.user_data = &app_data;

    // Launch engine loop (ZERO heap allocations during execution)
    bool success = Scry::Platform::RunEngine(&app);
    if (success) {
        printf("\nEngine loop execution completed successfully.\n");
    } else {
        printf("\nEngine loop failed to launch.\n");
    }

    return 0;
}
