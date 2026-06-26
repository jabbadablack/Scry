// clang-format off
#include "editor_renderer_bridge.hpp"
// clang-format on

#include <debug/assert.h>
#include <cstring>
#include <memory>

namespace editor {

bool RendererBridge::Initialize(engine::renderer::DiligentModule& renderer, uint32_t width, uint32_t height) {
    ENGINE_ASSERT(width > 0, "Offscreen viewport width must be positive");
    ENGINE_ASSERT(height > 0, "Offscreen viewport height must be positive");
    ENGINE_ASSERT(!m_setter, "RendererBridge::Initialize called after a setter was already registered");
    renderer.EnableOffscreenMode(width, height);
    renderer.SetFrameReadyCallback([this](const uint8_t* pData, uint32_t w, uint32_t h, uint32_t rowPitch) {
        OnFrameReady(pData, w, h, rowPitch);
    });
    return true;
}

void RendererBridge::SetImageSetter(ViewportImageSetter setter) {
    ENGINE_ASSERT(setter != nullptr, "Image setter callback must not be null");
    ENGINE_ASSERT(!m_setter, "Image setter already registered — call Shutdown() before replacing it");
    std::lock_guard<std::mutex> lock(m_setterMutex);
    m_setter = std::move(setter);
}

void RendererBridge::Shutdown() {
    std::lock_guard<std::mutex> lock(m_setterMutex);
    m_setter = nullptr;
}

void RendererBridge::OnFrameReady(const uint8_t* pData, uint32_t width, uint32_t height, uint32_t rowPitch) {
    ENGINE_ASSERT(pData != nullptr, "Frame readback pixel buffer must not be null");
    ENGINE_ASSERT(width > 0 && height > 0, "Frame readback dimensions must be positive");
    ENGINE_ASSERT(rowPitch >= width * 4u, "Row pitch must be at least width*4 bytes for RGBA8 data");
    // Build a tightly-packed SharedPixelBuffer from the (possibly padded) staging readback.
    // slint::Rgba8Pixel is { uint8_t r, g, b, a } which matches TEX_FORMAT_RGBA8_UNORM layout.
    slint::SharedPixelBuffer<slint::Rgba8Pixel> buffer(width, height);
    slint::Rgba8Pixel* dst = buffer.begin();

    for (uint32_t row = 0; row < height; ++row) {
        const uint8_t* src = pData + (static_cast<size_t>(row) * rowPitch);
        for (uint32_t col = 0; col < width; ++col) {
            dst->r = src[col * 4u + 0u];
            dst->g = src[col * 4u + 1u];
            dst->b = src[col * 4u + 2u];
            dst->a = src[col * 4u + 3u];
            ++dst;
        }
    }

    // slint::Image is constructed directly from a SharedPixelBuffer (no static factory).
    // Wrap in shared_ptr so the lambda captures a copyable, ref-counted handle.
    auto shared_image = std::make_shared<slint::Image>(std::move(buffer));

    ViewportImageSetter setter_copy;
    {
        std::lock_guard<std::mutex> lock(m_setterMutex);
        setter_copy = m_setter;
    }

    if (setter_copy) {
        // Dispatch to the Slint event-loop thread — safe to call from any thread.
        slint::invoke_from_event_loop([setter = std::move(setter_copy), img = std::move(shared_image)]() {
            setter(*img);
        });
    }
}

} // namespace editor
