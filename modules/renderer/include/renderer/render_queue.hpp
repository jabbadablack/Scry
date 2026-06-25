#ifndef ENGINE_RENDERER_RENDER_QUEUE_HPP
#define ENGINE_RENDERER_RENDER_QUEUE_HPP

#include <math/algebra.hpp>
#include <ecs/ecs_types.hpp>
#include <memory/chained_arena.hpp>
#include <graphics/render_packet.hpp>
#include <entt/entt.hpp>
#include <cstddef>


namespace engine::renderer {

    class RenderQueue {
    public:
        ENGINE_INLINE RenderQueue() = default;
        ENGINE_INLINE ~RenderQueue() = default;

        RenderQueue(const RenderQueue&) = delete;
        RenderQueue& operator=(const RenderQueue&) = delete;

        ENGINE_INLINE void Initialize(engine::ChainedArena& arena, size_t max_commands);
        ENGINE_INLINE void Push(const engine::graphics::RenderPacket& cmd);
        ENGINE_INLINE void Sort();

        [[nodiscard]] ENGINE_INLINE const engine::graphics::RenderPacket* GetCommands() const noexcept;
        [[nodiscard]] ENGINE_INLINE size_t GetCount() const noexcept;

    private:
        engine::graphics::RenderPacket* m_commands = nullptr;
        size_t m_count = 0;
        size_t m_capacity = 0;
    };

} // namespace engine::renderer


#include "render_queue.inl"

#endif // ENGINE_RENDERER_RENDER_QUEUE_HPP
