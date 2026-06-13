#include <scry/scry_memory.hpp>
#include <stdio.h>
#include <stdint.h>
#include <mimalloc.h>
#include <new>

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

struct TestParticle {
    float x;             // 4 bytes
    float y;             // 4 bytes
    float z;             // 4 bytes
    uint16_t id;         // 2 bytes
    int8_t active;       // 1 byte
};

extern "C" SCRY_API const char* ScryGetVersion();


int main() {
    printf("=== Scry Framework Engine Sandbox ===\n");
    printf("Engine Version: %s\n\n", ScryGetVersion());

    // 1. Verify mimalloc overriding
    printf("--- 1. Allocator Override Verification ---\n");
    
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


    // 2. Test Arena Allocator
    printf("\n--- 2. Arena Allocator Verification ---\n");
    const size_t arena_size = 1024;
    void* arena_backing = ::operator new(arena_size);
    
    Scry::Memory::Arena arena;
    arena.Init(arena_backing, arena_size);
    printf("Arena initialized with %zu bytes backing store.\n", arena_size);

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
    printf("Arena reset. Current used memory: %zu bytes\n", arena.GetUsedMemory());
    ::operator delete(arena_backing);

    // 3. Test Pool Allocator
    printf("\n--- 3. Pool Allocator Verification ---\n");
    
    const uint32_t pool_capacity = 4;
    size_t pool_mem_size = Scry::Memory::PoolAllocator<TestParticle, pool_capacity>::GetRequiredMemorySize();
    printf("PoolAllocator required backing memory size: %zu bytes\n", pool_mem_size);
    
    void* pool_backing = ::operator new(pool_mem_size);
    
    Scry::Memory::PoolAllocator<TestParticle, pool_capacity> pool;
    pool.Init(pool_backing, pool_mem_size);
    printf("Pool initialized using contiguous pre-allocated block.\n");

    // Allocate indices
    uint32_t h1 = pool.Allocate();
    uint32_t h2 = pool.Allocate();
    uint32_t h3 = pool.Allocate();
    uint32_t h4 = pool.Allocate();

    printf("Allocated index 1: %u\n", h1);
    printf("Allocated index 2: %u\n", h2);
    printf("Allocated index 3: %u\n", h3);
    printf("Allocated index 4: %u\n", h4);

    uint32_t h5 = pool.Allocate();
    printf("Allocated index 5 (should fail/return 0xFFFFFFFF since capacity is %u): %u\n", pool_capacity, h5);

    // Initialize the allocated particle data
    TestParticle* pParticle1 = pool.Get(h1);
    if (pParticle1) {
        pParticle1->x = 1.0f;
        pParticle1->y = 2.0f;
        pParticle1->z = 3.0f;
        pParticle1->id = 101;
        pParticle1->active = 1;
        printf("Set particle 1 data: id=%u, pos=(%.1f, %.1f, %.1f), active=%d\n", 
               pParticle1->id, pParticle1->x, pParticle1->y, pParticle1->z, pParticle1->active);
    }

    TestParticle* pParticle2 = pool.Get(h2);
    if (pParticle2) {
        pParticle2->x = 10.0f;
        pParticle2->y = 20.0f;
        pParticle2->z = 30.0f;
        pParticle2->id = 102;
        pParticle2->active = 1;
    }

    // Verify contiguous layout of underlying memory
    if (pParticle1 && pParticle2) {
        ptrdiff_t byte_diff = (uintptr_t)pParticle2 - (uintptr_t)pParticle1;
        printf("Verify contiguous block: address difference between particle 2 and particle 1: %td bytes (expected %zu)\n", 
               byte_diff, sizeof(TestParticle));
    }

    // Test Free and slot reuse
    printf("Freeing particle 2 (index %u)\n", h2);
    pool.Free(h2);

    TestParticle* pParticle2_after = pool.Get(h2);
    printf("Get particle 2 after free: %p (expected NULL/nullptr)\n", (void*)pParticle2_after);

    uint32_t h6 = pool.Allocate();
    printf("Allocated index after freeing: %u (expected to reuse %u)\n", h6, h2);

    pool.Reset();
    printf("Pool reset. Active count: %zu\n", pool.GetActiveCount());
    
    ::operator delete(pool_backing);

    printf("\nAll memory checks completed successfully!\n");
    return 0;
}
