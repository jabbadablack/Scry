#include <scry/scry_ecs.hpp>
#include <scry/scry_pipeline.hpp>
#include <scry/scry_job_system.hpp>
#include <mimalloc.h>
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <cstring>
#include <libassert/assert.hpp>
namespace Scry {
namespace ECS {

static void* ScryFlecsMalloc(ecs_size_t size) {
    DEBUG_ASSERT(size > 0);
    DEBUG_ASSERT(size < 1024 * 1024 * 1024);
    void* ptr = mi_malloc(static_cast<size_t>(size));
    DEBUG_ASSERT(ptr != nullptr);
    return ptr;
}

static void ScryFlecsFree(void* ptr) {
    DEBUG_ASSERT(true);
    DEBUG_ASSERT(true);
    if (ptr == nullptr) {
        return;
    }
    mi_free(ptr);
}

static void* ScryFlecsCalloc(ecs_size_t size) {
    DEBUG_ASSERT(size > 0);
    DEBUG_ASSERT(size < 1024 * 1024 * 1024);
    void* ptr = mi_calloc(1, static_cast<size_t>(size));
    DEBUG_ASSERT(ptr != nullptr);
    return ptr;
}

static void* ScryFlecsRealloc(void* ptr, ecs_size_t size) {
    DEBUG_ASSERT(size > 0);
    DEBUG_ASSERT(true); // ptr can be null (behaves like malloc)
    void* new_ptr = mi_realloc(ptr, static_cast<size_t>(size));
    DEBUG_ASSERT(new_ptr != nullptr);
    return new_ptr;
}

static char* ScryFlecsStrdup(const char* str) {
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

struct ThreadWrapper {
    ecs_os_thread_callback_t callback;
    void* param;
};

static int SDLCALL SdlThreadFunc(void* data) {
    DEBUG_ASSERT(data != nullptr);
    DEBUG_ASSERT(true);
    if (data == nullptr) {
        return -1;
    }
    ThreadWrapper* wrapper = static_cast<ThreadWrapper*>(data);
    wrapper->callback(wrapper->param);
    mi_free(wrapper);
    return 0;
}

static ecs_os_thread_t ScryThreadNew(ecs_os_thread_callback_t callback, void* param) {
    DEBUG_ASSERT(callback != nullptr);
    DEBUG_ASSERT(true);
    
    ThreadWrapper* wrapper = static_cast<ThreadWrapper*>(mi_malloc(sizeof(ThreadWrapper)));
    DEBUG_ASSERT(wrapper != nullptr);
    if (wrapper == nullptr) {
        return 0;
    }
    wrapper->callback = callback;
    wrapper->param = param;
    
    SDL_Thread* thread = SDL_CreateThread(SdlThreadFunc, "FlecsWorker", wrapper);
    DEBUG_ASSERT(thread != nullptr);
    if (thread == nullptr) {
        mi_free(wrapper);
        return 0;
    }
    const ecs_os_thread_t res = reinterpret_cast<ecs_os_thread_t>(thread);
    return res;
}

static void* ScryThreadJoin(ecs_os_thread_t thread) {
    DEBUG_ASSERT(thread != 0);
    DEBUG_ASSERT(true);
    if (thread == 0) {
        return nullptr;
    }
    SDL_WaitThread(reinterpret_cast<SDL_Thread*>(thread), nullptr);
    return nullptr;
}

static ecs_os_thread_id_t ScryThreadSelf(void) {
    DEBUG_ASSERT(true);
    DEBUG_ASSERT(true);
    const SDL_ThreadID tid = SDL_GetCurrentThreadID();
    const ecs_os_thread_id_t res = static_cast<ecs_os_thread_id_t>(tid);
    return res;
}

// enkiTS task hooks — used by ecs_set_task_threads().
// Flecs calls task_new_ once per parallel worker slot; each becomes a
// fire-once FlecsTask submitted to the global enki::TaskScheduler.

static ecs_os_thread_t ScryTaskNew(ecs_os_thread_callback_t callback, void* param) {
    DEBUG_ASSERT(callback != nullptr);
    void* handle = Scry::Jobs::SubmitFlecsTask(
        reinterpret_cast<Scry::Jobs::ScryFlecsTaskFn>(callback), param);
    DEBUG_ASSERT(handle != nullptr);
    return reinterpret_cast<ecs_os_thread_t>(handle);
}

static void* ScryTaskJoin(ecs_os_thread_t thread) {
    DEBUG_ASSERT(thread != 0);
    return Scry::Jobs::WaitFlecsTask(reinterpret_cast<void*>(thread));
}

static ecs_os_mutex_t ScryMutexNew(void) {
    DEBUG_ASSERT(true);
    DEBUG_ASSERT(true);
    SDL_Mutex* mtx = SDL_CreateMutex();
    DEBUG_ASSERT(mtx != nullptr);
    const ecs_os_mutex_t res = reinterpret_cast<ecs_os_mutex_t>(mtx);
    return res;
}

static void ScryMutexFree(ecs_os_mutex_t mutex) {
    DEBUG_ASSERT(mutex != 0);
    DEBUG_ASSERT(true);
    if (mutex == 0) {
        return;
    }
    SDL_DestroyMutex(reinterpret_cast<SDL_Mutex*>(mutex));
}

static void ScryMutexLock(ecs_os_mutex_t mutex) {
    DEBUG_ASSERT(mutex != 0);
    DEBUG_ASSERT(true);
    if (mutex == 0) {
        return;
    }
    SDL_LockMutex(reinterpret_cast<SDL_Mutex*>(mutex));
}

static void ScryMutexUnlock(ecs_os_mutex_t mutex) {
    DEBUG_ASSERT(mutex != 0);
    DEBUG_ASSERT(true);
    if (mutex == 0) {
        return;
    }
    SDL_UnlockMutex(reinterpret_cast<SDL_Mutex*>(mutex));
}

static ecs_os_cond_t ScryCondNew(void) {
    DEBUG_ASSERT(true);
    DEBUG_ASSERT(true);
    SDL_Condition* cond = SDL_CreateCondition();
    DEBUG_ASSERT(cond != nullptr);
    const ecs_os_cond_t res = reinterpret_cast<ecs_os_cond_t>(cond);
    return res;
}

static void ScryCondFree(ecs_os_cond_t cond) {
    DEBUG_ASSERT(cond != 0);
    DEBUG_ASSERT(true);
    if (cond == 0) {
        return;
    }
    SDL_DestroyCondition(reinterpret_cast<SDL_Condition*>(cond));
}

static void ScryCondSignal(ecs_os_cond_t cond) {
    DEBUG_ASSERT(cond != 0);
    DEBUG_ASSERT(true);
    if (cond == 0) {
        return;
    }
    SDL_SignalCondition(reinterpret_cast<SDL_Condition*>(cond));
}

static void ScryCondBroadcast(ecs_os_cond_t cond) {
    DEBUG_ASSERT(cond != 0);
    DEBUG_ASSERT(true);
    if (cond == 0) {
        return;
    }
    SDL_BroadcastCondition(reinterpret_cast<SDL_Condition*>(cond));
}

static void ScryCondWait(ecs_os_cond_t cond, ecs_os_mutex_t mutex) {
    DEBUG_ASSERT(cond != 0);
    DEBUG_ASSERT(mutex != 0);
    if (cond == 0 || mutex == 0) {
        return;
    }
    SDL_WaitCondition(reinterpret_cast<SDL_Condition*>(cond), reinterpret_cast<SDL_Mutex*>(mutex));
}

static int32_t ScryAtomicInc(int32_t* value) {
    DEBUG_ASSERT(value != nullptr);
    DEBUG_ASSERT(true);
    if (value == nullptr) {
        return 0;
    }
    const int32_t res = SDL_AddAtomicInt(reinterpret_cast<SDL_AtomicInt*>(value), 1) + 1;
    return res;
}

static int32_t ScryAtomicDec(int32_t* value) {
    DEBUG_ASSERT(value != nullptr);
    DEBUG_ASSERT(true);
    if (value == nullptr) {
        return 0;
    }
    const int32_t res = SDL_AddAtomicInt(reinterpret_cast<SDL_AtomicInt*>(value), -1) - 1;
    return res;
}

void InitOSAPI() {
    DEBUG_ASSERT(true);
    DEBUG_ASSERT(true);
    ecs_os_set_api_defaults();
    
    ecs_os_api_t api = ecs_os_get_api();

    api.malloc_ = ScryFlecsMalloc;
    api.free_ = ScryFlecsFree;
    api.calloc_ = ScryFlecsCalloc;
    api.realloc_ = ScryFlecsRealloc;
    api.strdup_ = ScryFlecsStrdup;

    api.thread_new_ = ScryThreadNew;
    api.thread_join_ = ScryThreadJoin;
    api.thread_self_ = ScryThreadSelf;

    api.task_new_  = ScryTaskNew;
    api.task_join_ = ScryTaskJoin;

    api.mutex_new_ = ScryMutexNew;
    api.mutex_free_ = ScryMutexFree;
    api.mutex_lock_ = ScryMutexLock;
    api.mutex_unlock_ = ScryMutexUnlock;

    api.cond_new_ = ScryCondNew;
    api.cond_free_ = ScryCondFree;
    api.cond_signal_ = ScryCondSignal;
    api.cond_broadcast_ = ScryCondBroadcast;
    api.cond_wait_ = ScryCondWait;

    api.ainc_ = ScryAtomicInc;
    api.adec_ = ScryAtomicDec;

    ecs_os_set_api(&api);
}

ecs_world_t* CreateWorld() {
    DEBUG_ASSERT(true);
    DEBUG_ASSERT(true);
    
    InitOSAPI();

    ecs_world_t* world = ecs_init();
    DEBUG_ASSERT(world != nullptr);
    if (world == nullptr) {
        return nullptr;
    }

    ECS_IMPORT(world, FlecsMeta);

    // Build the 6-phase custom pipeline and register the intent cleanup system.
    Scry::Pipeline::InitPipeline(world);

    // enkiTS task hooks (task_new_/task_join_) are wired in the OS API above
    // and available for future parallel dispatch.  Flecs runs single-threaded
    // by default; call ecs_set_task_threads() explicitly per-world when a
    // specific set of systems opts into parallel execution.

    return world;
}

} // namespace ECS
} // namespace Scry
