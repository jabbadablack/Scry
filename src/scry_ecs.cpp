#include <scry/scry_ecs.hpp>
#include <mimalloc.h>
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <cstring>

namespace Scry {
namespace ECS {

ecs_entity_t OnIntentPhase = 0;
ecs_entity_t OnStateUpdatePhase = 0;
ecs_entity_t OnReactPhase = 0;

// ============================================================================
// 1. Memory Hooks (mimalloc wrapper)
// ============================================================================
static void* ScryFlecsMalloc(ecs_size_t size) {
    return mi_malloc(static_cast<size_t>(size));
}

static void ScryFlecsFree(void* ptr) {
    mi_free(ptr);
}

static void* ScryFlecsCalloc(ecs_size_t size) {
    return mi_calloc(1, static_cast<size_t>(size));
}


static void* ScryFlecsRealloc(void* ptr, ecs_size_t size) {
    return mi_realloc(ptr, static_cast<size_t>(size));
}

static char* ScryFlecsStrdup(const char* str) {
    if (!str) return nullptr;
    size_t len = std::strlen(str) + 1;
    char* copy = static_cast<char*>(mi_malloc(len));
    if (copy) {
        std::memcpy(copy, str, len);
    }
    return copy;
}

// ============================================================================
// 2. Threading Hooks (SDL3 wrapper)
// ============================================================================
struct ThreadWrapper {
    ecs_os_thread_callback_t callback;
    void* param;
};

static int SDLCALL SdlThreadFunc(void* data) {
    ThreadWrapper* wrapper = static_cast<ThreadWrapper*>(data);
    wrapper->callback(wrapper->param);
    mi_free(wrapper);
    return 0;
}

static ecs_os_thread_t ScryThreadNew(ecs_os_thread_callback_t callback, void* param) {
    ThreadWrapper* wrapper = static_cast<ThreadWrapper*>(mi_malloc(sizeof(ThreadWrapper)));
    if (!wrapper) return 0;
    wrapper->callback = callback;
    wrapper->param = param;
    
    SDL_Thread* thread = SDL_CreateThread(SdlThreadFunc, "FlecsWorker", wrapper);
    return reinterpret_cast<ecs_os_thread_t>(thread);
}

static void* ScryThreadJoin(ecs_os_thread_t thread) {
    SDL_WaitThread(reinterpret_cast<SDL_Thread*>(thread), nullptr);
    return nullptr;
}

static ecs_os_thread_id_t ScryThreadSelf(void) {
    return static_cast<ecs_os_thread_id_t>(SDL_GetCurrentThreadID());
}

// ============================================================================
// 3. Mutex Hooks (SDL3 wrapper)
// ============================================================================
static ecs_os_mutex_t ScryMutexNew(void) {
    return reinterpret_cast<ecs_os_mutex_t>(SDL_CreateMutex());
}

static void ScryMutexFree(ecs_os_mutex_t mutex) {
    SDL_DestroyMutex(reinterpret_cast<SDL_Mutex*>(mutex));
}

static void ScryMutexLock(ecs_os_mutex_t mutex) {
    SDL_LockMutex(reinterpret_cast<SDL_Mutex*>(mutex));
}

static void ScryMutexUnlock(ecs_os_mutex_t mutex) {
    SDL_UnlockMutex(reinterpret_cast<SDL_Mutex*>(mutex));
}

// ============================================================================
// 4. Condition Variable Hooks (SDL3 wrapper)
// ============================================================================
static ecs_os_cond_t ScryCondNew(void) {
    return reinterpret_cast<ecs_os_cond_t>(SDL_CreateCondition());
}

static void ScryCondFree(ecs_os_cond_t cond) {
    SDL_DestroyCondition(reinterpret_cast<SDL_Condition*>(cond));
}

static void ScryCondSignal(ecs_os_cond_t cond) {
    SDL_SignalCondition(reinterpret_cast<SDL_Condition*>(cond));
}

static void ScryCondBroadcast(ecs_os_cond_t cond) {
    SDL_BroadcastCondition(reinterpret_cast<SDL_Condition*>(cond));
}

static void ScryCondWait(ecs_os_cond_t cond, ecs_os_mutex_t mutex) {
    SDL_WaitCondition(reinterpret_cast<SDL_Condition*>(cond), reinterpret_cast<SDL_Mutex*>(mutex));
}

// ============================================================================
// 5. Atomic Hooks (SDL3 wrapper)
// ============================================================================
static int32_t ScryAtomicInc(int32_t* value) {
    // SDL_AddAtomicInt returns the previous value, so add 1 to get the new value.
    return SDL_AddAtomicInt(reinterpret_cast<SDL_AtomicInt*>(value), 1) + 1;
}

static int32_t ScryAtomicDec(int32_t* value) {
    // SDL_AddAtomicInt returns the previous value, so subtract 1 to get the new value.
    return SDL_AddAtomicInt(reinterpret_cast<SDL_AtomicInt*>(value), -1) - 1;
}

// ============================================================================
// OS API Initialization
// ============================================================================
void InitOSAPI() {
    ecs_os_set_api_defaults();
    ecs_os_api_t api = ecs_os_get_api();

    // Override Memory Allocators
    api.malloc_ = ScryFlecsMalloc;
    api.free_ = ScryFlecsFree;
    api.calloc_ = ScryFlecsCalloc;
    api.realloc_ = ScryFlecsRealloc;
    api.strdup_ = ScryFlecsStrdup;

    // Override Threading
    api.thread_new_ = ScryThreadNew;
    api.thread_join_ = ScryThreadJoin;
    api.thread_self_ = ScryThreadSelf;

    // Override Mutexes
    api.mutex_new_ = ScryMutexNew;
    api.mutex_free_ = ScryMutexFree;
    api.mutex_lock_ = ScryMutexLock;
    api.mutex_unlock_ = ScryMutexUnlock;

    // Override Condition Variables
    api.cond_new_ = ScryCondNew;
    api.cond_free_ = ScryCondFree;
    api.cond_signal_ = ScryCondSignal;
    api.cond_broadcast_ = ScryCondBroadcast;
    api.cond_wait_ = ScryCondWait;

    // Override Atomics
    api.ainc_ = ScryAtomicInc;
    api.adec_ = ScryAtomicDec;

    ecs_os_set_api(&api);
}

ecs_world_t* CreateWorld() {
    // Override the Flecs OS API before initializing the ecs_world_t
    InitOSAPI();

    ecs_world_t* world = ecs_init();
    if (!world) {
        return nullptr;
    }

    // Configure pipeline phases: OnIntent, OnStateUpdate, OnReact
    ecs_entity_desc_t desc = {};

    desc.name = "OnIntent";
    OnIntentPhase = ecs_entity_init(world, &desc);
    ecs_add_pair(world, OnIntentPhase, EcsDependsOn, EcsPreUpdate);

    desc.name = "OnStateUpdate";
    OnStateUpdatePhase = ecs_entity_init(world, &desc);
    ecs_add_pair(world, OnStateUpdatePhase, EcsDependsOn, OnIntentPhase);

    desc.name = "OnReact";
    OnReactPhase = ecs_entity_init(world, &desc);
    ecs_add_pair(world, OnReactPhase, EcsDependsOn, OnStateUpdatePhase);

    return world;
}


} // namespace ECS
} // namespace Scry
