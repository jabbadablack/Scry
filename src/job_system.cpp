#include <engine/job_system.hpp>
#include <engine/memory.hpp>
#include <engine/ecs.hpp>
#include <libassert/assert.hpp>
#include <TaskScheduler.h>
#include <mimalloc.h>
#include <stdio.h>

namespace Engine {

namespace ECS {
// Forward declare so Jobs::Shutdown can check the ThreadWrapper pool.
extern Memory::PoolAllocator g_thread_wrapper_pool;
}

namespace Jobs {

static enki::TaskScheduler g_scheduler;

static Memory::PoolAllocator g_task_pool;
static void* g_task_pool_mem = nullptr;
static const uint32_t MAX_FLECS_TASKS = 256;

struct FlecsTask : enki::ITaskSet {
    FlecsTaskFn callback;
    void*           param;
    void*           result = nullptr;
    uint32_t        pool_index = 0;

    FlecsTask(FlecsTaskFn cb, void* p, uint32_t idx)
        : enki::ITaskSet(1), callback(cb), param(p), pool_index(idx) {}

    void ExecuteRange(enki::TaskSetPartition, uint32_t) override {
        result = callback(param);
    }
};

bool Init(uint32_t thread_count) {
    // 0 = "use minimum": 1 worker thread. N = N worker threads (N+1 total with main).
    const uint32_t total = (thread_count == 0) ? 2u : thread_count + 1u;
    g_scheduler.Initialize(total);

#ifndef NDEBUG
    char buf[64];
    std::snprintf(buf, sizeof(buf), "[Jobs] Scheduler started: %u total threads (%u workers)",
        total, total - 1u);
    EngineLog(buf);
#endif

    size_t req_size = Memory::PoolGetRequiredSize(sizeof(FlecsTask), MAX_FLECS_TASKS);
    g_task_pool_mem = mi_malloc(req_size);
    DEBUG_ASSERT(g_task_pool_mem != nullptr);
    if (g_task_pool_mem == nullptr) {
        EngineLog("[Jobs] FATAL: failed to allocate FlecsTask pool");
        return false;
    }

    Memory::PoolInit(&g_task_pool, g_task_pool_mem, req_size, sizeof(FlecsTask), MAX_FLECS_TASKS);

#ifndef NDEBUG
    EngineLog("[Jobs] FlecsTask pool ready");
#endif

    return true;
}

void Shutdown() {
    g_scheduler.WaitforAllAndShutdown();

    // NASA Rule Check for both pools
    if (Memory::PoolGetActiveCount(&g_task_pool) > 0) {
        fprintf(stderr, "[Engine] CRITICAL MEMORY LEAK: FlecsTask pool has active allocations!\n");
    }

    if (g_task_pool_mem) {
        mi_free(g_task_pool_mem);
        g_task_pool_mem = nullptr;
    }
}

void SubmitTask(TaskFn fn, void* user_data, uint32_t count) {
    DEBUG_ASSERT(fn != nullptr);
    DEBUG_ASSERT(count > 0);

    struct JobTaskSet : enki::ITaskSet {
        TaskFn fn;
        void* user_data;
        void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override {
            fn(range.start, range.end, threadnum, user_data);
        }
    };

    JobTaskSet task;
    task.fn        = fn;
    task.user_data = user_data;
    task.m_SetSize = count;

    g_scheduler.AddTaskSetToPipe(&task);
    g_scheduler.WaitforTask(&task);
}

void* SubmitFlecsTask(FlecsTaskFn callback, void* param) {
    DEBUG_ASSERT(callback != nullptr);
    uint32_t idx = Memory::PoolAllocate(&g_task_pool);
    DEBUG_ASSERT(idx != 0xFFFFFFFF);
    if (idx == 0xFFFFFFFF) return nullptr;

    void* mem = Memory::PoolGet(&g_task_pool, idx);
    FlecsTask* task = new (mem) FlecsTask(callback, param, idx);
    g_scheduler.AddTaskSetToPipe(task);
    return task;
}

void* WaitFlecsTask(void* handle) {
    DEBUG_ASSERT(handle != nullptr);
    if (handle == nullptr) {
        return nullptr;
    }
    FlecsTask* task = static_cast<FlecsTask*>(handle);
    g_scheduler.WaitforTask(task);
    void* result = task->result;
    uint32_t idx = task->pool_index;
    task->~FlecsTask();
    Memory::PoolFree(&g_task_pool, idx);
    return result;
}

uint32_t GetTotalThreadCount() {
    return g_scheduler.GetNumTaskThreads() + 1u;
}

enki::TaskScheduler* GetScheduler() {
    return &g_scheduler;
}

} // namespace Jobs
} // namespace Engine
