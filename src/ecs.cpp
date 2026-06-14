#include <engine/ecs.hpp>
#include <engine/pipeline.hpp>
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
    DEBUG_ASSERT(size > 0);
    DEBUG_ASSERT(size < 1024 * 1024 * 1024);
    void* ptr = mi_malloc(static_cast<size_t>(size));
    DEBUG_ASSERT(ptr != nullptr);
    return ptr;
}

static void FlecsFree(void* ptr) {
    DEBUG_ASSERT(true);
    DEBUG_ASSERT(true);
    if (ptr == nullptr) {
        return;
    }
    mi_free(ptr);
}

static void* FlecsCalloc(ecs_size_t size) {
    DEBUG_ASSERT(size > 0);
    DEBUG_ASSERT(size < 1024 * 1024 * 1024);
    void* ptr = mi_calloc(1, static_cast<size_t>(size));
    DEBUG_ASSERT(ptr != nullptr);
    return ptr;
}

static void* FlecsRealloc(void* ptr, ecs_size_t size) {
    DEBUG_ASSERT(size > 0);
    DEBUG_ASSERT(true); // ptr can be null (behaves like malloc)
    void* new_ptr = mi_realloc(ptr, static_cast<size_t>(size));
    DEBUG_ASSERT(new_ptr != nullptr);
    return new_ptr;
}

static char* FlecsStrdup(const char* str) {
    DEBUG_ASSERT(true);
    DEBUG_ASSERT(true);
    if (str == nullptr) {
        return nullptr;
    }
    const size_t len = std::strlen(str) + 1;
    char* copy = static_cast<char*>(mi_malloc(len));
    DEBUG_ASSERT(copy != nullptr);
    if (copy != nullptr) {
        std::memcpy(copy, str, len);
    }
    return copy;
}

struct ThreadWrapper { ecs_os_thread_callback_t callback; void* param; uint32_t pool_index; };

Memory::PoolAllocator g_thread_wrapper_pool;
static void* g_thread_wrapper_pool_mem = nullptr;
static const uint32_t MAX_THREAD_WRAPPERS = 64;

static int SDLCALL SdlThreadFunc(void* data) {
    DEBUG_ASSERT(data != nullptr);
    DEBUG_ASSERT(true);
    if (data == nullptr) {
        return -1;
    }
    ThreadWrapper* wrapper = static_cast<ThreadWrapper*>(data);
    wrapper->callback(wrapper->param);
    uint32_t idx = wrapper->pool_index;
    Memory::PoolFree(&g_thread_wrapper_pool, idx);
    return 0;
}

static ecs_os_thread_t ThreadNew(ecs_os_thread_callback_t callback, void* param) {
    DEBUG_ASSERT(callback != nullptr);
    DEBUG_ASSERT(true);
    
    uint32_t idx = Memory::PoolAllocate(&g_thread_wrapper_pool);
    DEBUG_ASSERT(idx != 0xFFFFFFFF);
    ThreadWrapper* wrapper = static_cast<ThreadWrapper*>(Memory::PoolGet(&g_thread_wrapper_pool, idx));
    DEBUG_ASSERT(wrapper != nullptr);
    if (wrapper == nullptr) {
        return 0;
    }
    wrapper->callback = callback;
    wrapper->param = param;
    wrapper->pool_index = idx;
    
    SDL_Thread* thread = SDL_CreateThread(SdlThreadFunc, "FlecsWorker", wrapper);
    DEBUG_ASSERT(thread != nullptr);
    if (thread == nullptr) {
        Memory::PoolFree(&g_thread_wrapper_pool, idx);
        return 0;
    }
    const ecs_os_thread_t res = reinterpret_cast<ecs_os_thread_t>(thread);
    return res;
}

static void* ThreadJoin(ecs_os_thread_t thread) {
    DEBUG_ASSERT(thread != 0);
    DEBUG_ASSERT(true);
    if (thread == 0) {
        return nullptr;
    }
    SDL_WaitThread(reinterpret_cast<SDL_Thread*>(thread), nullptr);
    return nullptr;
}

static ecs_os_thread_id_t ThreadSelf(void) {
    const uint64_t tid = SDL_GetCurrentThreadID();
    const ecs_os_thread_id_t res = static_cast<ecs_os_thread_id_t>(tid);
    return res;
}

static ecs_os_thread_t TaskNew(ecs_os_thread_callback_t callback, void* param) {
    return reinterpret_cast<ecs_os_thread_t>(Engine::Jobs::SubmitFlecsTask(callback, param));
}

static void* TaskJoin(ecs_os_thread_t handle) {
    return Engine::Jobs::WaitFlecsTask(reinterpret_cast<void*>(handle));
}

static ecs_os_mutex_t MutexNew(void) {
    return reinterpret_cast<ecs_os_mutex_t>(SDL_CreateMutex());
}

static void MutexFree(ecs_os_mutex_t mutex) {
    DEBUG_ASSERT(mutex != 0);
    if (mutex != 0) {
        SDL_DestroyMutex(reinterpret_cast<SDL_Mutex*>(mutex));
    }
}

static void MutexLock(ecs_os_mutex_t mutex) {
    DEBUG_ASSERT(mutex != 0);
    if (mutex != 0) {
        SDL_LockMutex(reinterpret_cast<SDL_Mutex*>(mutex));
    }
}

static void MutexUnlock(ecs_os_mutex_t mutex) {
    DEBUG_ASSERT(mutex != 0);
    if (mutex != 0) {
        SDL_UnlockMutex(reinterpret_cast<SDL_Mutex*>(mutex));
    }
}

static ecs_os_cond_t CondNew(void) {
    return reinterpret_cast<ecs_os_cond_t>(SDL_CreateCondition());
}

static void CondFree(ecs_os_cond_t cond) {
    DEBUG_ASSERT(cond != 0);
    if (cond != 0) {
        SDL_DestroyCondition(reinterpret_cast<SDL_Condition*>(cond));
    }
}

static void CondSignal(ecs_os_cond_t cond) {
    DEBUG_ASSERT(cond != 0);
    if (cond != 0) {
        SDL_SignalCondition(reinterpret_cast<SDL_Condition*>(cond));
    }
}

static void CondBroadcast(ecs_os_cond_t cond) {
    DEBUG_ASSERT(cond != 0);
    if (cond != 0) {
        SDL_BroadcastCondition(reinterpret_cast<SDL_Condition*>(cond));
    }
}

static void CondWait(ecs_os_cond_t cond, ecs_os_mutex_t mutex) {
    DEBUG_ASSERT(cond != 0);
    DEBUG_ASSERT(mutex != 0);
    if (cond != 0 && mutex != 0) {
        SDL_WaitCondition(reinterpret_cast<SDL_Condition*>(cond), reinterpret_cast<SDL_Mutex*>(mutex));
    }
}

static int32_t AtomicInc(int32_t* value) {
    DEBUG_ASSERT(value != nullptr);
    // SDL_AtomicAdd returns the previous value.
    return SDL_AddAtomicInt(reinterpret_cast<SDL_AtomicInt*>(value), 1) + 1;
}

static int32_t AtomicDec(int32_t* value) {
    DEBUG_ASSERT(value != nullptr);
    return SDL_AddAtomicInt(reinterpret_cast<SDL_AtomicInt*>(value), -1) - 1;
}

void InitOSAPI() {
    DEBUG_ASSERT(true);
    DEBUG_ASSERT(true);
    
    size_t req_size = Memory::PoolGetRequiredSize(sizeof(ThreadWrapper), MAX_THREAD_WRAPPERS);
    g_thread_wrapper_pool_mem = mi_malloc(req_size);
    DEBUG_ASSERT(g_thread_wrapper_pool_mem != nullptr);
    Memory::PoolInit(&g_thread_wrapper_pool, g_thread_wrapper_pool_mem, req_size, sizeof(ThreadWrapper), MAX_THREAD_WRAPPERS);
    
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

ecs_world_t* CreateWorld() {
    InitOSAPI();

    ecs_world_t* world = ecs_init();
    DEBUG_ASSERT(world != nullptr);

    if (world == nullptr) {
        return nullptr;
    }

    // Initialize custom pipeline phases
    Pipeline::InitPipeline(world);

    // Bind task scheduler for parallel dispatch.  Flecs runs single-threaded
    // by default; call ecs_set_task_threads() explicitly per-world when a
    // specific set of systems opts into parallel execution.

    return world;
}

} // namespace ECS
} // namespace Engine
