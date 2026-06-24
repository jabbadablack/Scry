#ifndef ENGINE_CORE_ENGINE_HPP
#define ENGINE_CORE_ENGINE_HPP

#include "OS/window_manager.hpp"
#include "OS/IInput.hpp"
#include "ecs/registry.hpp"
#include "IO/manager.hpp"
#include "IO/vfs.hpp"
#include "IO/threading/job_system.hpp"
#include "module/module.hpp"
#include "memory/chained_arena.hpp"
#include "intent/intent_queue.hpp"
#include "time/ITime.hpp"
#include "graphics/IGraphics.hpp"
#include <vector>
#include <memory>
#include <mutex>
#include <taskflow/taskflow.hpp>

namespace engine {

    class Engine {
    public:
        // Constructor & Destructor
        ENGINE_INLINE Engine();
        ENGINE_INLINE ~Engine();

        // Disable copy and move semantics entirely
        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;
        Engine(Engine&&) = delete;
        Engine& operator=(Engine&&) = delete;

        // Core Engine API
        ENGINE_INLINE bool Initialize(IInput* input);
        ENGINE_INLINE bool Initialize(int width = 800, int height = 600, const char* title = "Engine Runtime");
        ENGINE_INLINE void Tick();
        ENGINE_INLINE void SetInterpolationAlpha(double alpha);
        ENGINE_INLINE void Shutdown();
        ENGINE_INLINE void RebuildExecutionGraph();
        ENGINE_INLINE void Run();

        // Execute Intent Frame to generate intents
        ENGINE_INLINE void ExecuteIntentFrame();

        // Templated API to register modules
        template <typename T, typename... Args>
        ENGINE_INLINE T& RegisterModule(Args&&... args);

        // State accessors
        [[nodiscard]] ENGINE_INLINE engine::WindowManager& GetWindowManager() noexcept { return m_windowManager; }
        [[nodiscard]] IInput* GetInput() const noexcept { return m_input; }

        [[nodiscard]] ENGINE_INLINE engine::io::ResourceManager& GetResourceManager() noexcept;
        [[nodiscard]] ENGINE_INLINE const engine::io::ResourceManager& GetResourceManager() const noexcept;

        [[nodiscard]] ENGINE_INLINE engine::io::JobSystem& GetJobSystem() noexcept;
        [[nodiscard]] ENGINE_INLINE engine::VirtualFileSystem& GetVFS() noexcept;

        [[nodiscard]] ENGINE_INLINE ecs::Registry& GetRegistry() noexcept;
        [[nodiscard]] ENGINE_INLINE engine::ChainedArena& GetFrameArena() noexcept;
        [[nodiscard]] ENGINE_INLINE tf::Taskflow& GetTaskflow() noexcept;
        [[nodiscard]] ENGINE_INLINE const ecs::Registry& GetRegistry() const noexcept;
        [[nodiscard]] ENGINE_INLINE const engine::ITime& GetTime() const noexcept { return m_ITime; }
        [[nodiscard]] ENGINE_INLINE engine::graphics::IGraphics& GetIGraphics() noexcept { return m_IGraphics; }

        // Double-buffer and thread-sync accessors
        [[nodiscard]] ENGINE_INLINE int GetWriteState() const noexcept { return m_writeState; }
        [[nodiscard]] ENGINE_INLINE int GetRenderReadState() const noexcept { return m_renderReadState; }
        [[nodiscard]] ENGINE_INLINE std::mutex& GetRenderSwapMutex() noexcept { return m_renderSwapMutex; }
        [[nodiscard]] ENGINE_INLINE engine::ChainedArena& GetFrameArena(int idx) noexcept;
        [[nodiscard]] ENGINE_INLINE float GetInterpolationAlpha() const noexcept { return static_cast<float>(m_interpolationAlpha); }

    private:
        engine::WindowManager m_windowManager;
        IInput* m_input = nullptr;
        engine::io::JobSystem m_jobSystem;
        engine::VirtualFileSystem m_vfs;
        engine::io::ResourceManager m_resourceManager;
        engine::graphics::IGraphics m_IGraphics;
        engine::ITime m_ITime;
        ecs::Registry m_registry;
        tf::Taskflow m_taskflow;
        std::vector<std::unique_ptr<IModule>> m_modules;
        engine::ChainedArena m_frameArenas[2];

        int m_writeState = 0;
        int m_renderReadState = 1;
        std::mutex m_renderSwapMutex;
        double m_interpolationAlpha = 0.0;

        std::unique_ptr<IWindow> m_defaultWindow;
        std::unique_ptr<IInput> m_defaultInput;
    };

} // namespace engine

#include "engine.inl"

#endif // ENGINE_CORE_ENGINE_HPP
