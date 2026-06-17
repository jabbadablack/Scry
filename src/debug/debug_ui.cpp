#ifdef SCRY_DEBUG

#include <engine/debug/debug_ui.h>
#include <engine/renderer/core.h>
#include <engine/transform.h>   // id_ScryWorldMatrix for entity count

#include <ImGuiImplDiligent.hpp>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>

#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <flecs.h>

#include <cstdio>
#include <cfloat>

using namespace Diligent;

// ── State ─────────────────────────────────────────────────────────────────────
static bool               g_ShowUI     = false;
static ImGuiImplDiligent* g_pImGuiImpl = nullptr;
static ecs_world_t*       g_World      = nullptr;

// ── Telemetry — ring buffers (TELEM_SAMPLES frames of history) ────────────────
static constexpr int TELEM_SAMPLES = 120;

static float  s_FrameTimes[TELEM_SAMPLES] = {};   // wall-clock ms per frame
static float  s_SysTimes  [TELEM_SAMPLES] = {};   // ECS system-callback ms
static float  s_EcsOvhd   [TELEM_SAMPLES] = {};   // ecs_progress overhead ms
static float  s_RenderMs  [TELEM_SAMPLES] = {};   // non-ECS ms (GPU, present, …)
static int    s_TelemOffset               = 0;

static double      s_LastWallTime   = -1.0;
static float       s_SmoothFps      = 0.0f;

// Cumulative ECS counters tracked frame-to-frame (delta gives per-frame values).
static ecs_ftime_t s_LastSystemTime = 0.f;
static ecs_ftime_t s_LastFrameTime  = 0.f;   // frame_time_total from world info

extern "C" {

void DebugUI_Init(void* window, void* device, void* /*context*/) {
    IRenderDevice* dev = static_cast<IRenderDevice*>(device);
    ISwapChain*    sc  = static_cast<ISwapChain*>(ScryGraphics_GetSwapChain());

    // ImGuiImplDiligent ctor calls ImGui::CreateContext() internally — calling it
    // ourselves first would leave a context with GLFW already installed when the
    // ctor fires its own CreateContext(), triggering the BackendPlatformUserData assert.
    const SwapChainDesc& scDesc = sc->GetDesc();
    g_pImGuiImpl = new ImGuiImplDiligent(ImGuiDiligentCreateInfo{
        dev, scDesc.ColorBufferFormat, TEX_FORMAT_UNKNOWN
    });

    // Context is now live — configure style then install the GLFW platform backend.
    IMGUI_CHECKVERSION();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow*>(window), true);
}

void DebugUI_Shutdown(void) {
    // Platform backend must be removed BEFORE the dtor calls DestroyContext(),
    // which asserts that BackendPlatformUserData == NULL.
    ImGui_ImplGlfw_Shutdown();
    delete g_pImGuiImpl;
    g_pImGuiImpl = nullptr;
}

void DebugUI_Toggle   (void) { g_ShowUI = !g_ShowUI; }
bool DebugUI_IsVisible(void) { return g_ShowUI; }

void DebugUI_SetWorld(void* ecs_world) {
    g_World = static_cast<ecs_world_t*>(ecs_world);
}

void DebugUI_NewFrame(void) {
    ISwapChain* sc = static_cast<ISwapChain*>(ScryGraphics_GetSwapChain());
    const SwapChainDesc& scDesc = sc->GetDesc();

    // Wall-clock timing runs every frame so the ring buffer is always warm when
    // the panel is first opened.
    double now = glfwGetTime();
    if (s_LastWallTime >= 0.0) {
        float dt_ms = (float)((now - s_LastWallTime) * 1000.0);
        s_FrameTimes[s_TelemOffset] = dt_ms;
        if (dt_ms > 0.001f)
            s_SmoothFps = 0.95f * s_SmoothFps + 0.05f * (1000.f / dt_ms);
    }
    s_LastWallTime = now;

    // GLFW backend sets io.DisplaySize — must run before ImGuiImplDiligent::NewFrame,
    // which immediately calls ImGui::NewFrame() and asserts DisplaySize >= 0.
    ImGui_ImplGlfw_NewFrame();
    g_pImGuiImpl->NewFrame(scDesc.Width, scDesc.Height, SURFACE_TRANSFORM_IDENTITY);
}

void DebugUI_Render(void) {
    if (g_ShowUI) {
        if (ImGui::Begin("Scry Engine")) {
            ImGui::Text("F1 — toggle this panel");
            ImGui::Separator();

            // ── Frame timing (wall clock) ──────────────────────────────────────
            ImGui::SeparatorText("Frame Timing");
            {
                float sum = 0.f;
                for (int i = 0; i < TELEM_SAMPLES; ++i) sum += s_FrameTimes[i];
                float avg = sum / (float)TELEM_SAMPLES;

                char ov[32]; snprintf(ov, sizeof(ov), "avg %.2f ms", avg);
                ImGui::PlotLines("##wall", s_FrameTimes, TELEM_SAMPLES, s_TelemOffset,
                                 ov, 0.f, 33.3f, ImVec2(-FLT_MIN, 60.f));
                ImGui::Text("FPS: %.1f  |  Frame: %.2f ms", s_SmoothFps, avg);
            }

            // ── ECS / ISR phase breakdown ──────────────────────────────────────
            if (g_World) {
                ImGui::SeparatorText("ISR Pipeline");

                const ecs_world_info_t* info = ecs_get_world_info(g_World);

                // Per-frame deltas from cumulative counters
                float frame_ms   = s_FrameTimes[s_TelemOffset]; // written by NewFrame this tick
                float sys_ms     = (info->system_time_total - s_LastSystemTime) * 1000.f;
                float ecs_ms     = (info->frame_time_total  - s_LastFrameTime)  * 1000.f;
                float ecs_ovhd   = ecs_ms - sys_ms;
                float render_ms  = frame_ms - ecs_ms;
                s_LastSystemTime = info->system_time_total;
                s_LastFrameTime  = info->frame_time_total;

                // Clamp negatives from measurement noise
                if (sys_ms    < 0.f) sys_ms    = 0.f;
                if (ecs_ovhd  < 0.f) ecs_ovhd  = 0.f;
                if (render_ms < 0.f) render_ms  = 0.f;

                s_SysTimes [s_TelemOffset] = sys_ms;
                s_EcsOvhd  [s_TelemOffset] = ecs_ovhd;
                s_RenderMs [s_TelemOffset] = render_ms;

                // Three PlotHistograms side by side showing how the 16.6 ms budget is spent
                float col_w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) / 3.f;
                ImVec2 hist_sz(col_w, 60.f);
                float budget = 16.6f;

                {
                    char ov[24]; snprintf(ov, sizeof(ov), "React %.2f", sys_ms);
                    ImGui::PlotHistogram("##sys", s_SysTimes, TELEM_SAMPLES, s_TelemOffset,
                                        ov, 0.f, budget, hist_sz);
                }
                ImGui::SameLine();
                {
                    char ov[24]; snprintf(ov, sizeof(ov), "ECS %.2f", ecs_ovhd);
                    ImGui::PlotHistogram("##ov", s_EcsOvhd, TELEM_SAMPLES, s_TelemOffset,
                                        ov, 0.f, budget, hist_sz);
                }
                ImGui::SameLine();
                {
                    char ov[24]; snprintf(ov, sizeof(ov), "GPU %.2f", render_ms);
                    ImGui::PlotHistogram("##gpu", s_RenderMs, TELEM_SAMPLES, s_TelemOffset,
                                        ov, 0.f, budget, hist_sz);
                }
                ImGui::TextDisabled("React (sys)       ECS overhead      GPU / Resolve");

                ImGui::Spacing();

                // ── ECS world stats ────────────────────────────────────────────
                ImGui::SeparatorText("ECS World");
                int32_t entity_count = ecs_count_id(g_World, (ecs_entity_t)id_ScryWorldMatrix);
                ImGui::Text("Entities (w/ WorldMatrix): %d", entity_count);
                ImGui::Text("Archetypes: %d  |  Sys/frame: %lld",
                            info->table_count,
                            (long long)info->systems_ran_total);
                ImGui::Text("Frame: #%lld", (long long)info->frame_count_total);
            }
        }
        ImGui::End();

        ImGui::ShowMetricsWindow();
    }

    // Advance the ring-buffer offset AFTER filling all slots for this frame.
    s_TelemOffset = (s_TelemOffset + 1) % TELEM_SAMPLES;

    // ImGui::Render() MUST be called unconditionally every frame — it flushes
    // internal memory arenas. ImGuiImplDiligent::Render() calls it internally,
    // and RenderDrawData() is a no-op when the draw list is empty (UI hidden),
    // so there is no GPU cost when g_ShowUI is false.
    IDeviceContext* ctx  = static_cast<IDeviceContext*>(ScryGraphics_GetContext());
    ISwapChain*     sc   = static_cast<ISwapChain*>(ScryGraphics_GetSwapChain());
    ITextureView*   pRTV = sc->GetCurrentBackBufferRTV();
    ctx->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    g_pImGuiImpl->Render(ctx);
}

} // extern "C"

#endif // SCRY_DEBUG
