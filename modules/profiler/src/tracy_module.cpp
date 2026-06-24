#include "profiler/tracy_module.hpp"
#include <engine.hpp>
#include <debug/logger.hpp>
#include <debug/assert.h>
#include <tracy/Tracy.hpp>

namespace engine {
namespace profiler {

    bool TracyModule::Initialize(engine::Engine& engine) {
        ENGINE_ASSERT(true, "TracyModule::Initialize pre-condition check");
        TracyMessageL("Tracy Profiler Initialized");
        ENGINE_LOG_INFO("TracyModule: profiler initialized");
        return true;
    }

    void TracyModule::BuildGraph(tf::Taskflow& taskflow) {
        // Tracy does not inject static task nodes
    }

    void TracyModule::CompileFrameGraph(FrameDAG& dag) {
        ENGINE_ASSERT(dag.write_state == 0 || dag.write_state == 1, "TracyModule: invalid write state in FrameDAG");
        ENGINE_ASSERT(dag.read_state == 0 || dag.read_state == 1, "TracyModule: invalid read state in FrameDAG");

        tf::Task tracy_task = dag.taskflow.emplace([]() {
            FrameMark;
        }).name("TracyFrameMark");

        dag.phase_extract.precede(tracy_task);
    }

    void TracyModule::Shutdown() {
        ENGINE_ASSERT(true, "TracyModule::Shutdown pre-condition check");
        TracyMessageL("Tracy Profiler Shutdown");
        ENGINE_LOG_INFO("TracyModule: profiler shut down");
    }

} // namespace profiler
} // namespace engine
