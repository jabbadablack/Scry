#ifndef ENGINE_PROFILER_TRACY_MODULE_HPP
#define ENGINE_PROFILER_TRACY_MODULE_HPP

#include <module/module.hpp>

namespace engine {

    class Engine;
}

namespace engine {
namespace profiler {

    class TracyModule : public engine::IModule {
    public:
        TracyModule() = default;
        ~TracyModule() override = default;

        const char* GetName() const override {
            return "TracyModule";
        }

        bool Initialize(engine::Engine& engine) override;
        void BuildGraph(tf::Taskflow& taskflow) override;
        void CompileFrameGraph(FrameDAG& dag) override;
        void Shutdown() override;
    };

} // namespace profiler
} // namespace engine

#endif // ENGINE_PROFILER_TRACY_MODULE_HPP
