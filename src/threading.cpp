#include <engine/threading.h>
#include <engine/engine.h>

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Engine {
namespace Threading {

// ── Task pool internals ───────────────────────────────────────────────────────

struct TaskEntry {
    uintptr_t                id;
    ecs_os_thread_callback_t fn;
    void*                    ctx;
};

static struct Pool {
    std::mutex                             mtx;
    std::condition_variable                cv_work; // workers wait here
    std::condition_variable                cv_done; // joiners wait here
    std::queue<TaskEntry>                  queue;
    std::unordered_map<uintptr_t, void*>   results; // id -> fn return value
    std::vector<std::thread>               workers;
    std::atomic<uintptr_t>                 next_id{1};
    bool                                   stopping = false;
} g_pool;

static void WorkerLoop() {
    while (true) {
        TaskEntry task{};
        {
            std::unique_lock<std::mutex> lk(g_pool.mtx);
            g_pool.cv_work.wait(lk, [] {
                return g_pool.stopping || !g_pool.queue.empty();
            });
            if (g_pool.stopping && g_pool.queue.empty()) return;
            task = g_pool.queue.front();
            g_pool.queue.pop();
        }
        void* result = task.fn(task.ctx);
        {
            std::lock_guard<std::mutex> lk(g_pool.mtx);
            g_pool.results[task.id] = result;
        }
        g_pool.cv_done.notify_all();
    }
}

static uintptr_t PushTask(ecs_os_thread_callback_t fn, void* ctx) {
    const uintptr_t id = g_pool.next_id.fetch_add(1, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lk(g_pool.mtx);
        g_pool.queue.push({id, fn, ctx});
    }
    g_pool.cv_work.notify_one();
    return id;
}

static void* JoinTask(uintptr_t id) {
    std::unique_lock<std::mutex> lk(g_pool.mtx);
    g_pool.cv_done.wait(lk, [id] {
        return g_pool.results.count(id) != 0;
    });
    void* result = g_pool.results.at(id);
    g_pool.results.erase(id);
    return result;
}

// ── Public API ────────────────────────────────────────────────────────────────

void Init(int num_threads) {
    assert(g_pool.workers.empty());
    if (num_threads < 1) num_threads = 1;
    g_pool.workers.reserve(static_cast<size_t>(num_threads));
    for (int i = 0; i < num_threads; ++i)
        g_pool.workers.emplace_back(WorkerLoop);
    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "[Threading] Pool started (%d threads)", num_threads);
        EngineLog(buf);
    }
}

void Shutdown() {
    {
        std::lock_guard<std::mutex> lk(g_pool.mtx);
        g_pool.stopping = true;
    }
    g_pool.cv_work.notify_all();
    for (auto& t : g_pool.workers) t.join();
    g_pool.workers.clear();
    g_pool.stopping  = false;
    g_pool.results.clear();
    g_pool.next_id.store(1, std::memory_order_relaxed);
    EngineLog("[Threading] Pool shut down");
}

void SetFlecsOSAPI() {
    // Copy the current OS API (populated by ecs_os_set_api_defaults()) and
    // patch only the task callbacks so Flecs' own thread_new_/thread_join_
    // (platform defaults) remain intact for ecs_set_threads().
    ecs_os_api_t api = ecs_os_api;
    api.task_new_ = [](ecs_os_thread_callback_t fn, void* ctx) -> ecs_os_thread_t {
        return static_cast<ecs_os_thread_t>(PushTask(fn, ctx));
    };
    api.task_join_ = [](ecs_os_thread_t handle) -> void* {
        return JoinTask(static_cast<uintptr_t>(handle));
    };
    ecs_os_set_api(&api);
    EngineLog("[Threading] Flecs task_new_/task_join_ bound to thread pool");
}

} // namespace Threading
} // namespace Engine
