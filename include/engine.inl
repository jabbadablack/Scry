#ifndef ENGINE_CORE_ENGINE_INL
#define ENGINE_CORE_ENGINE_INL

#include "engine.hpp"
#include "debug/logger.hpp"
#include <thread>

namespace engine {

    // Constructor
    ENGINE_INLINE Engine::Engine()
        : m_input(nullptr)
        , m_jobSystem()
        , m_vfs()
        , m_resourceManager(m_jobSystem, m_vfs)
        , m_registry()
        , m_taskflow()
        , m_modules()
        , m_frameArenas{ChainedArena(64 * 1024), ChainedArena(64 * 1024)}
    {
        static struct TrackedHeapLeakSentinel {
            ~TrackedHeapLeakSentinel() noexcept {
                engine::TrackedHeap::AssertNoLeaks();
            }
        } s_sentinel;
    }

    // Destructor: calls Shutdown()
    ENGINE_INLINE Engine::~Engine() {
        Shutdown();
    }

    // Initialize all registered modules with given input interface
    ENGINE_INLINE bool Engine::Initialize(IInput* input) {
        ENGINE_ASSERT(m_writeState == 0, "Engine double-initialized: write state is dirty");
        ENGINE_ASSERT(m_renderReadState == 1, "Engine double-initialized: render read state is dirty");

        bool mounted = m_vfs.AutoMount("res://", "assets");
        ENGINE_ASSERT(mounted, "VFS failed to mount 'res://' — 'assets' folder not found");
        if (!mounted) {
            ENGINE_LOG_ERROR("Engine::Initialize — VFS could not locate 'assets' folder");
            return false;
        }

        m_input = input;
        ENGINE_LOG_INFO("Engine initializing");

        ENGINE_LOG_INFO("Engine: Registering module reflection types");
        for (auto& module : m_modules) {
            module->RegisterReflection();
        }

        for (auto& module : m_modules) {
            ENGINE_ASSERT(module != nullptr, "Null module registered in Engine");
            ENGINE_ASSERT(module->GetName() != nullptr, "Module returned null name");
            ENGINE_LOG_INFO("Initializing module: " + std::string(module->GetName()));
            if (!module->Initialize(*this)) {
                ENGINE_LOG_ERROR("Module initialization failed: " + std::string(module->GetName()));
                return false;
            }
            ENGINE_LOG_INFO("Module initialized: " + std::string(module->GetName()));
        }

        for (auto& module : m_modules) {
            module->BuildGraph(m_taskflow);
        }

        ENGINE_LOG_INFO("Engine initialized");
        return true;
    }

    // Processes exactly one frame iteration
    ENGINE_INLINE void Engine::Tick() {
        ENGINE_ASSERT(m_writeState == 0 || m_writeState == 1, "Write state index out of range");
        ENGINE_ASSERT(!m_modules.empty(), "Engine::Tick called with no registered modules");

        m_resourceManager.Update();

        m_writeState = 1 - m_writeState;
        m_frameArenas[m_writeState].Clear();

        if (m_input) {
            m_input->Update();
        }

        m_taskflow.clear();

        // Create absolute synchronization barriers
        tf::Task phase_intent = m_taskflow.emplace([](){}).name("BARRIER_Intent");
        tf::Task phase_reactor = m_taskflow.emplace([](){}).name("BARRIER_Reactor");
        tf::Task phase_extract = m_taskflow.emplace([](){}).name("BARRIER_Extract");

        // Enforce ISR architectural order
        phase_intent.precede(phase_reactor);
        phase_reactor.precede(phase_extract);

        FrameDAG dag{m_taskflow, phase_intent, phase_reactor, phase_extract, m_writeState, m_renderReadState};

        for (auto& module : m_modules) {
            module->CompileFrameGraph(dag);
        }

        // 3. Execute the DAG
        m_jobSystem.GetExecutor().run(m_taskflow).wait();
    }

    // Sets interpolation alpha and updates read/write double buffer sync states
    ENGINE_INLINE void Engine::SetInterpolationAlpha(double alpha) {
        ENGINE_ASSERT(alpha >= 0.0, "Interpolation alpha must be >= 0.0");
        ENGINE_ASSERT(alpha <= 1.0, "Interpolation alpha must be <= 1.0");

        std::lock_guard<std::mutex> lock(m_renderSwapMutex);
        m_renderReadState = m_writeState;
        m_interpolationAlpha = alpha;
    }

    // Execute Intent Frame to generate intents
    ENGINE_INLINE void Engine::ExecuteIntentFrame() {
        // Hook for intent-generation systems
    }

    // Shutdown Engine and clean up resources
    ENGINE_INLINE void Engine::Shutdown() {
        ENGINE_ASSERT(m_writeState == 0 || m_writeState == 1, "Write state corrupted at shutdown");
        ENGINE_ASSERT(m_renderReadState == 0 || m_renderReadState == 1, "Render read state corrupted at shutdown");

        ENGINE_LOG_INFO("Engine shutting down");

        for (auto it = m_modules.rbegin(); it != m_modules.rend(); ++it) {
            ENGINE_ASSERT(*it != nullptr, "Null module encountered during shutdown");
            ENGINE_LOG_INFO("Shutting down module: " + std::string((*it)->GetName()));
            (*it)->Shutdown();
            ENGINE_LOG_INFO("Module shut down: " + std::string((*it)->GetName()));
        }
        m_modules.clear();

        ENGINE_LOG_INFO("Engine shutdown complete");
        engine::debug::AsyncLogger::Get().Shutdown();
    }

    // Rebuild the multithreaded frame execution taskflow graph
    ENGINE_INLINE void Engine::RebuildExecutionGraph() {
        ENGINE_ASSERT(!m_modules.empty(), "RebuildExecutionGraph called with no registered modules");
        ENGINE_ASSERT(m_writeState == 0 || m_writeState == 1, "Write state out of range at graph rebuild");

        m_taskflow.clear();
        for (auto& module : m_modules) {
            module->BuildGraph(m_taskflow);
        }
    }

    // Register module template implementation
    template <typename T, typename... Args>
    ENGINE_INLINE T& Engine::RegisterModule(Args&&... args) {
        auto module = std::make_unique<T>(std::forward<Args>(args)...);
        ENGINE_ASSERT(module != nullptr, "Failed to allocate module");
        ENGINE_ASSERT(module->GetName() != nullptr, "Registered module returned null name");

        T& ref = *module;
        m_modules.push_back(std::move(module));
        return ref;
    }

    // Resource Manager accessors
    ENGINE_INLINE engine::io::ResourceManager& Engine::GetResourceManager() noexcept {
        return m_resourceManager;
    }

    ENGINE_INLINE const engine::io::ResourceManager& Engine::GetResourceManager() const noexcept {
        return m_resourceManager;
    }

    // Job System accessors
    ENGINE_INLINE engine::io::JobSystem& Engine::GetJobSystem() noexcept {
        return m_jobSystem;
    }

    // VFS accessors
    ENGINE_INLINE engine::VirtualFileSystem& Engine::GetVFS() noexcept {
        return m_vfs;
    }

    // ChainedArena accessors
    ENGINE_INLINE engine::ChainedArena& Engine::GetFrameArena() noexcept {
        return m_frameArenas[m_writeState];
  }

    // Taskflow accessor
    ENGINE_INLINE tf::Taskflow& Engine::GetTaskflow() noexcept {
        return m_taskflow;
    }

    // Registry accessors
    ENGINE_INLINE ecs::Registry& Engine::GetRegistry() noexcept {
        return m_registry;
    }

    ENGINE_INLINE const ecs::Registry& Engine::GetRegistry() const noexcept {
        return m_registry;
    }

    // ChainedArena by index accessor
    ENGINE_INLINE engine::ChainedArena& Engine::GetFrameArena(int idx) noexcept {
        ENGINE_ASSERT(idx >= 0, "Frame arena index must be non-negative");
        ENGINE_ASSERT(idx < 2, "Frame arena index out of bounds (max 1)");
        return m_frameArenas[idx];
    }

} // namespace engine

#endif // ENGINE_CORE_ENGINE_INL
