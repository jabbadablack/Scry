#ifndef ENGINE_RENDERER_RENDER_QUEUE_INL
#define ENGINE_RENDERER_RENDER_QUEUE_INL

#include "render_queue.hpp"
#include <debug/assert.h>

namespace engine {
namespace renderer {

    ENGINE_INLINE void RenderQueue::Initialize(engine::ChainedArena& arena, size_t max_commands) {
        ENGINE_ASSERT(max_commands > 0, "RenderQueue capacity must be greater than zero");

        m_commands = reinterpret_cast<RenderCommand*>(
            arena.Allocate(max_commands * sizeof(RenderCommand), alignof(RenderCommand))
        );
        ENGINE_ASSERT(m_commands != nullptr, "Failed to allocate memory from ChainedArena for RenderQueue");
        m_count = 0;
        m_capacity = max_commands;
    }

    ENGINE_INLINE void RenderQueue::Push(const RenderCommand& cmd) {
        ENGINE_ASSERT(m_commands != nullptr, "Cannot push to uninitialized RenderQueue");
        ENGINE_ASSERT(m_count < m_capacity, "RenderQueue capacity exhausted");
        m_commands[m_count] = cmd;
        m_count++;
    }

    ENGINE_INLINE const RenderCommand* RenderQueue::GetCommands() const noexcept {
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

} // namespace renderer
} // namespace engine

#endif // ENGINE_RENDERER_RENDER_QUEUE_INL
