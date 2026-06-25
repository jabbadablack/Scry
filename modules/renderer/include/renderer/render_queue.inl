#ifndef ENGINE_RENDERER_RENDER_QUEUE_INL
#define ENGINE_RENDERER_RENDER_QUEUE_INL

#include "render_queue.hpp"
#include <algorithm>
#include <debug/assert.h>


namespace engine::renderer {

    ENGINE_INLINE void RenderQueue::Initialize(engine::ChainedArena& arena, size_t max_commands) {
        ENGINE_ASSERT(max_commands > 0, "RenderQueue capacity must be greater than zero");

        m_commands = reinterpret_cast<engine::graphics::RenderPacket*>(
            arena.Allocate(max_commands * sizeof(engine::graphics::RenderPacket), alignof(engine::graphics::RenderPacket))
        );
        ENGINE_ASSERT(m_commands != nullptr, "Failed to allocate memory from ChainedArena for RenderQueue");
        m_count = 0;
        m_capacity = max_commands;
    }

    ENGINE_INLINE void RenderQueue::Push(const engine::graphics::RenderPacket& cmd) {
        ENGINE_ASSERT(m_commands != nullptr, "Cannot push to uninitialized RenderQueue");
        ENGINE_ASSERT(m_count < m_capacity, "RenderQueue capacity exhausted");
        m_commands[m_count] = cmd;
        m_count++;
    }

    ENGINE_INLINE const engine::graphics::RenderPacket* RenderQueue::GetCommands() const noexcept {
        ENGINE_ASSERT(m_commands != nullptr, "GetCommands called on uninitialized RenderQueue");
        ENGINE_ASSERT(m_capacity > 0, "GetCommands called on zero-capacity RenderQueue");
        return m_commands;
    }

    ENGINE_INLINE size_t RenderQueue::GetCount() const noexcept {
        if (m_commands == nullptr) return 0;
        ENGINE_ASSERT(m_capacity > 0, "RenderQueue has buffer but zero capacity — initialization bug");
        ENGINE_ASSERT(m_count <= m_capacity, "RenderQueue count exceeds capacity — memory corruption");
        return m_count;
    }

    ENGINE_INLINE void RenderQueue::Sort() {
        if (m_count > 1) {
            std::stable_sort(m_commands, m_commands + m_count, 
                [](const engine::graphics::RenderPacket& a, const engine::graphics::RenderPacket& b) {
                    // Primary sort by Pass, Secondary sort by Key (Material/Depth)
                    if (a.pass != b.pass) return a.pass < b.pass;
                    return a.sort_key < b.sort_key;
                });
        }
    }

    ENGINE_INLINE void RenderQueue::SetCameraViewProj(const engine::math::Matrix4& vp) noexcept {
        m_cameraViewProj = vp;
    }

    ENGINE_INLINE const engine::math::Matrix4& RenderQueue::GetCameraViewProj() const noexcept {
        return m_cameraViewProj;
    }

} // namespace engine::renderer


#endif // ENGINE_RENDERER_RENDER_QUEUE_INL
