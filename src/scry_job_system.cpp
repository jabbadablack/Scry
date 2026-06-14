#include <scry/scry_job_system.hpp>
#include <libassert/assert.hpp>
#include <TaskScheduler.h>

namespace Scry {
namespace Jobs {

static enki::TaskScheduler g_scheduler;

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

} // namespace Jobs
} // namespace Scry
