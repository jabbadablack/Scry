#include <scry/scry_job_system.hpp>
#include <libassert/assert.hpp>
#include <TaskScheduler.h>
#include <mimalloc.h>

namespace Scry {
namespace Jobs {

static enki::TaskScheduler g_scheduler;

// One-shot wrapper submitted to enkiTS in response to Flecs task_new_ calls.
// m_SetSize = 1 ensures ExecuteRange fires exactly once on a worker thread.
struct FlecsTask : enki::ITaskSet {
    ScryFlecsTaskFn callback;
    void*           param;
    void*           result = nullptr;

    FlecsTask(ScryFlecsTaskFn cb, void* p)
        : enki::ITaskSet(1), callback(cb), param(p) {}

    void ExecuteRange(enki::TaskSetPartition, uint32_t) override {
        result = callback(param);
    }
};

bool Init() {
    g_scheduler.Initialize();
    return true;
}

void Shutdown() {
    g_scheduler.WaitforAllAndShutdown();
}

void SubmitTask(ScryTaskFn fn, void* user_data, uint32_t count) {
    DEBUG_ASSERT(fn != nullptr);
    DEBUG_ASSERT(count > 0);

    struct JobTaskSet : enki::ITaskSet {
        ScryTaskFn fn;
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

void* SubmitFlecsTask(ScryFlecsTaskFn callback, void* param) {
    DEBUG_ASSERT(callback != nullptr);
    void* mem = mi_malloc(sizeof(FlecsTask));
    DEBUG_ASSERT(mem != nullptr);
    FlecsTask* task = new (mem) FlecsTask(callback, param);
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
    task->~FlecsTask();
    mi_free(task);
    return result;
}

uint32_t GetTotalThreadCount() {
    // enkiTS GetNumTaskThreads() excludes the main thread; add 1 for the full pool.
    return g_scheduler.GetNumTaskThreads() + 1u;
}

enki::TaskScheduler* GetScheduler() {
    return &g_scheduler;
}

} // namespace Jobs
} // namespace Scry
