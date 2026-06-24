#ifndef ENGINE_IMGUI_IMGUI_MODULE_HPP
#define ENGINE_IMGUI_IMGUI_MODULE_HPP

#include <module/module.hpp>
#include <imgui.h>

namespace engine {

    class Engine;
}

namespace engine {
namespace imgui {

    class ImGuiModule : public engine::IModule {
    public:
        ImGuiModule() = default;
        ~ImGuiModule() override = default;

        const char* GetName() const override {
            return "ImGuiModule";
        }

        bool Initialize(engine::Engine& engine) override;
        void BuildGraph(tf::Taskflow& taskflow) override;
        void CompileFrameGraph(FrameDAG& dag) override;
        void Shutdown() override;
    };

} // namespace imgui
} // namespace engine

#endif // ENGINE_IMGUI_IMGUI_MODULE_HPP
