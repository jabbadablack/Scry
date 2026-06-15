#include <engine/ecs.hpp>
#include <engine/pipeline.hpp>
#include <engine/transform.hpp>
#include <engine/job_system.hpp>
#include <engine/memory.hpp>
#include <mimalloc.h>
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <cstring>
#include <libassert/assert.hpp>

namespace Engine {
namespace ECS {

static void* FlecsMalloc(ecs_size_t size) {
    return mi_malloc(static_cast<size_t>(size));
}

static void FlecsFree(void* ptr) {
    if (ptr) mi_free(ptr);
}

static void* FlecsCalloc(ecs_size_t size) {
    return mi_calloc(1, static_cast<size_t>(size));
}

static void* FlecsRealloc(void* ptr, ecs_size_t size) {
    return mi_realloc(ptr, static_cast<size_t>(size));
}

static char* FlecsStrdup(const char* str) {
    if (str == nullptr) return nullptr;
    const size_t len = std::strlen(str) + 1;
    char* copy = static_cast<char*>(mi_malloc(len));
    if (copy) std::memcpy(copy, str, len);
    return copy;
}

struct ThreadWrapper { ecs_os_thread_callback_t callback; void* param; uint32_t pool_index; };

Memory::PoolAllocator g_thread_wrapper_pool;
static void* g_thread_wrapper_pool_mem = nullptr;
static const uint32_t MAX_THREAD_WRAPPERS = 64;

static int SDLCALL SdlThreadFunc(void* data) {
    if (data == nullptr) return -1;
    ThreadWrapper* wrapper = static_cast<ThreadWrapper*>(data);
    wrapper->callback(wrapper->param);
    uint32_t idx = wrapper->pool_index;
    Memory::PoolFree(&g_thread_wrapper_pool, idx);
    return 0;
}

static ecs_os_thread_t ThreadNew(ecs_os_thread_callback_t callback, void* param) {
    uint32_t idx = Memory::PoolAllocate(&g_thread_wrapper_pool);
    if (idx == 0xFFFFFFFF) return 0;

    ThreadWrapper* wrapper = static_cast<ThreadWrapper*>(Memory::PoolGet(&g_thread_wrapper_pool, idx));
    wrapper->callback = callback;
    wrapper->param = param;
    wrapper->pool_index = idx;
    
    SDL_Thread* thread = SDL_CreateThread(SdlThreadFunc, "FlecsWorker", wrapper);
    if (thread == nullptr) {
        Memory::PoolFree(&g_thread_wrapper_pool, idx);
        return 0;
    }
    return reinterpret_cast<ecs_os_thread_t>(thread);
}

static void* ThreadJoin(ecs_os_thread_t thread) {
    if (thread == 0) return nullptr;
    SDL_WaitThread(reinterpret_cast<SDL_Thread*>(thread), nullptr);
    return nullptr;
}

static ecs_os_thread_id_t ThreadSelf(void) {
    return static_cast<ecs_os_thread_id_t>(SDL_GetCurrentThreadID());
}

static ecs_os_thread_t TaskNew(ecs_os_thread_callback_t callback, void* param) {
    return reinterpret_cast<ecs_os_thread_t>(Engine::Jobs::SubmitFlecsTask(
        reinterpret_cast<Engine::Jobs::FlecsTaskFn>(callback), param));
}

static void* TaskJoin(ecs_os_thread_t handle) {
    return Engine::Jobs::WaitFlecsTask(reinterpret_cast<void*>(handle));
}

static ecs_os_mutex_t MutexNew(void) {
    return reinterpret_cast<ecs_os_mutex_t>(SDL_CreateMutex());
}

static void MutexFree(ecs_os_mutex_t mutex) {
    if (mutex) SDL_DestroyMutex(reinterpret_cast<SDL_Mutex*>(mutex));
}

static void MutexLock(ecs_os_mutex_t mutex) {
    if (mutex) SDL_LockMutex(reinterpret_cast<SDL_Mutex*>(mutex));
}

static void MutexUnlock(ecs_os_mutex_t mutex) {
    if (mutex) SDL_UnlockMutex(reinterpret_cast<SDL_Mutex*>(mutex));
}

static ecs_os_cond_t CondNew(void) {
    return reinterpret_cast<ecs_os_cond_t>(SDL_CreateCondition());
}

static void CondFree(ecs_os_cond_t cond) {
    if (cond) SDL_DestroyCondition(reinterpret_cast<SDL_Condition*>(cond));
}

static void CondSignal(ecs_os_cond_t cond) {
    if (cond) SDL_SignalCondition(reinterpret_cast<SDL_Condition*>(cond));
}

static void CondBroadcast(ecs_os_cond_t cond) {
    if (cond) SDL_BroadcastCondition(reinterpret_cast<SDL_Condition*>(cond));
}

static void CondWait(ecs_os_cond_t cond, ecs_os_mutex_t mutex) {
    if (cond && mutex) SDL_WaitCondition(reinterpret_cast<SDL_Condition*>(cond), reinterpret_cast<SDL_Mutex*>(mutex));
}

static int32_t AtomicInc(int32_t* value) {
    return SDL_AddAtomicInt(reinterpret_cast<SDL_AtomicInt*>(value), 1) + 1;
}

static int32_t AtomicDec(int32_t* value) {
    return SDL_AddAtomicInt(reinterpret_cast<SDL_AtomicInt*>(value), -1) - 1;
}

void InitOSAPI() {
    if (g_thread_wrapper_pool_mem == nullptr) {
        size_t req_size = Memory::PoolGetRequiredSize(sizeof(ThreadWrapper), MAX_THREAD_WRAPPERS);
        g_thread_wrapper_pool_mem = mi_malloc(req_size);
        DEBUG_ASSERT(g_thread_wrapper_pool_mem != nullptr);
        Memory::PoolInit(&g_thread_wrapper_pool, g_thread_wrapper_pool_mem, req_size, sizeof(ThreadWrapper), MAX_THREAD_WRAPPERS);
    }
    
    ecs_os_set_api_defaults();
    ecs_os_api_t api = ecs_os_api;

    api.malloc_  = FlecsMalloc;
    api.free_    = FlecsFree;
    api.calloc_  = FlecsCalloc;
    api.realloc_ = FlecsRealloc;
    api.strdup_  = FlecsStrdup;

    api.thread_new_  = ThreadNew;
    api.thread_join_ = ThreadJoin;
    api.thread_self_ = ThreadSelf;

    api.task_new_  = TaskNew;
    api.task_join_ = TaskJoin;

    api.mutex_new_ = MutexNew;
    api.mutex_free_ = MutexFree;
    api.mutex_lock_ = MutexLock;
    api.mutex_unlock_ = MutexUnlock;

    api.cond_new_ = CondNew;
    api.cond_free_ = CondFree;
    api.cond_signal_ = CondSignal;
    api.cond_broadcast_ = CondBroadcast;
    api.cond_wait_ = CondWait;

    api.ainc_ = AtomicInc;
    api.adec_ = AtomicDec;

    ecs_os_set_api(&api);
}

void ShutdownOSAPI() {
    if (g_thread_wrapper_pool_mem) {
        mi_free(g_thread_wrapper_pool_mem);
        g_thread_wrapper_pool_mem = nullptr;
    }
}

ecs_world_t* CreateWorld() {
    InitOSAPI();

    ecs_world_t* world = ecs_init();
    DEBUG_ASSERT(world != nullptr);
    if (world == nullptr) {
        EngineLog("[ECS] FATAL: ecs_init() returned null");
        return nullptr;
    }

#ifndef NDEBUG
    EngineLog("[ECS] World created");
#endif

    Pipeline::InitPipeline(world);

#ifndef NDEBUG
    EngineLog("[ECS] Pipeline ready");
#endif

    Transform::Init(world);

    return world;
}

} // namespace ECS
} // namespace Engine
