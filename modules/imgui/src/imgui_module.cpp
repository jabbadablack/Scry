#include "imgui/imgui_module.hpp"
#include <engine.hpp>
#include <debug/logger.hpp>
#include <debug/assert.h>

namespace engine {
namespace imgui {

    bool ImGuiModule::Initialize(engine::Engine& engine) {
        ENGINE_ASSERT(ImGui::GetCurrentContext() == nullptr, "ImGuiModule::Initialize called with an existing ImGui context");

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ENGINE_ASSERT(ImGui::GetCurrentContext() != nullptr, "ImGuiModule: failed to create ImGui context");

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        // NOTE: Rendering ImGui via bgfx requires the standard imgui_impl_bgfx.cpp backend.
        // The user must bind their specific platform and renderer backend implementations here,
        // e.g., ImGui_ImplGlfw_InitForOther(window, true) and ImGui_ImplBgfx_Init().

        ENGINE_LOG_INFO("ImGuiModule: initialized");
        return true;
    }

    void ImGuiModule::BuildGraph(tf::Taskflow& taskflow) {
        // ImGui does not inject static task nodes
    }

    void ImGuiModule::CompileFrameGraph(FrameDAG& dag) {
        ENGINE_ASSERT(ImGui::GetCurrentContext() != nullptr, "ImGuiModule::CompileFrameGraph called without an active ImGui context");
        ENGINE_ASSERT(dag.write_state == 0 || dag.write_state == 1, "ImGuiModule: invalid write state in FrameDAG");

        tf::Task new_frame_task = dag.taskflow.emplace([]() {
            ImGui::NewFrame();
        }).name("ImGuiNewFrame");

        tf::Task render_task = dag.taskflow.emplace([]() {
            ImGui::Render();
            // Trigger backend draw submission here, e.g., ImGui_ImplBgfx_RenderDrawData(ImGui::GetDrawData());
        }).name("ImGuiRender");

        new_frame_task.precede(dag.phase_intent);
        dag.phase_extract.precede(render_task);
    }

    void ImGuiModule::Shutdown() {
        ENGINE_ASSERT(ImGui::GetCurrentContext() != nullptr, "ImGuiModule::Shutdown called without an active ImGui context");

        ImGui::DestroyContext();

        ENGINE_ASSERT(ImGui::GetCurrentContext() == nullptr, "ImGuiModule: context not destroyed after Shutdown");
        ENGINE_LOG_INFO("ImGuiModule: shut down");
    }

} // namespace imgui
} // namespace engine
