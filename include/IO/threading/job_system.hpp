#ifndef ENGINE_IO_THREADING_JOB_SYSTEM_HPP
#define ENGINE_IO_THREADING_JOB_SYSTEM_HPP

#include <memory>
#include <stdexcept>
#include <utility>
#include <taskflow/taskflow.hpp>
#include "../../OS/types.h"

namespace engine {
namespace io {

    class JobSystem {
    public:
        ENGINE_INLINE JobSystem();
        ENGINE_INLINE ~JobSystem();

        // Disable copy and move
        JobSystem(const JobSystem&) = delete;
        JobSystem& operator=(const JobSystem&) = delete;

        ENGINE_INLINE tf::Executor& GetExecutor();

        template <typename F>
        ENGINE_INLINE auto RunTask(F&& func);

        template <typename F>
        ENGINE_INLINE void RunTaskSilent(F&& func);

    private:
        tf::Executor m_executor;
    };

} // namespace io
} // namespace engine

#include "job_system.inl"

#endif // ENGINE_IO_THREADING_JOB_SYSTEM_HPP
