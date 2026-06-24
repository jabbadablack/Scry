#ifndef ENGINE_IO_THREADING_JOB_SYSTEM_INL
#define ENGINE_IO_THREADING_JOB_SYSTEM_INL


namespace engine::io {

    ENGINE_INLINE JobSystem::JobSystem() {
        ENGINE_ASSERT(m_executor.num_workers() > 0, "JobSystem executor initialized with zero worker threads");
    }

    ENGINE_INLINE JobSystem::~JobSystem() {
        ENGINE_ASSERT(m_executor.num_workers() > 0, "JobSystem executor invalid at destruction");
    }

    ENGINE_INLINE tf::Executor& JobSystem::GetExecutor() {
        ENGINE_ASSERT(m_executor.num_workers() > 0, "GetExecutor called on a zero-worker executor");
        return m_executor;
    }

    template <typename F>
    ENGINE_INLINE auto JobSystem::RunTask(F&& func) {
        ENGINE_ASSERT(m_executor.num_workers() > 0, "RunTask called on a zero-worker executor");
        return m_executor.async(std::forward<F>(func));
    }

    template <typename F>
    ENGINE_INLINE void JobSystem::RunTaskSilent(F&& func) {
        ENGINE_ASSERT(m_executor.num_workers() > 0, "RunTaskSilent called on a zero-worker executor");
        m_executor.silent_async(std::forward<F>(func));
    }

} // namespace engine::io


#endif // ENGINE_IO_THREADING_JOB_SYSTEM_INL
