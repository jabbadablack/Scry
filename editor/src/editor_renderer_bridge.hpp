#pragma once

// clang-format off
#include <renderer/diligent_module.hpp>
#include <slint.h>
// clang-format on

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

namespace editor {

// RendererBridge decouples the Diligent render thread from the Slint UI thread.
//
// Each frame the render thread calls back with a raw RGBA8 pixel pointer from a
// CPU-mapped staging texture.  The bridge converts that to a slint::Image via a
// SharedPixelBuffer and dispatches it to the Slint event-loop via
// slint::invoke_from_event_loop so that the viewport Image property is updated on
// Slint's thread without any further synchronisation on the caller's side.
class RendererBridge {
public:
    using ViewportImageSetter = std::function<void(slint::Image)>;

    RendererBridge() = default;
    ~RendererBridge() = default;

    RendererBridge(const RendererBridge&) = delete;
    RendererBridge& operator=(const RendererBridge&) = delete;
    RendererBridge(RendererBridge&&) = delete;
    RendererBridge& operator=(RendererBridge&&) = delete;

    // Must be called after DiligentModule::Initialize().
    // Enables offscreen rendering in the renderer and registers the readback callback.
    bool Initialize(engine::renderer::DiligentModule& renderer, uint32_t width, uint32_t height);

    // Provide the Slint property setter.  Called from the main thread before Run().
    void SetImageSetter(ViewportImageSetter setter);

    // Clear the callback and release the setter reference — safe to call from any thread.
    void Shutdown();

private:
    // Invoked from the DiligentModule render thread after each CPU readback.
    // pData  — pointer to the mapped RGBA8 staging texture data (valid only during this call)
    // width  — image width in pixels
    // height — image height in pixels
    // rowPitch — byte stride between rows (may be > width * 4 due to GPU alignment padding)
    void OnFrameReady(const uint8_t* pData, uint32_t width, uint32_t height, uint32_t rowPitch);

    ViewportImageSetter m_setter;
    std::mutex m_setterMutex;
};

} // namespace editor
