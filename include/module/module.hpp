#ifndef ENGINE_CORE_MODULE_HPP
#define ENGINE_CORE_MODULE_HPP

#include <taskflow/taskflow.hpp>

namespace engine {


    class Engine;

    struct FrameDAG {
        tf::Taskflow& taskflow;
        tf::Task phase_intent;
        tf::Task phase_reactor;
        tf::Task phase_extract;
        int write_state;
        int read_state;
    };

    class IModule {
    public:
        virtual ~IModule() = default;

        // Called once at startup
        virtual bool Initialize(Engine& engine) = 0;

        // Called once to allow the module to inject its execution nodes into the global DAG
        virtual void BuildGraph(tf::Taskflow& taskflow) = 0;

        // Called once at startup to register entt::meta reflection types (optional)
        virtual void RegisterReflection() {}

        virtual void CompileFrameGraph(FrameDAG& dag) {}

        // Called on engine exit
        virtual void Shutdown() = 0;

        // For logging/debugging
        [[nodiscard]] virtual const char* GetName() const = 0;
    };


} // namespace engine

#endif // ENGINE_CORE_MODULE_HPP
