#ifndef ENGINE_RENDERER_RENDER_QUEUE_HPP
#define ENGINE_RENDERER_RENDER_QUEUE_HPP

#include <math/algebra.hpp>
#include <memory/chained_arena.hpp>
#include <entt/entt.hpp>
#include <cstddef>

namespace engine {
namespace renderer {

    struct RenderCommand {
        engine::math::Matrix4 transform;
        engine::math::Matrix4 previous_transform;
        entt::hashed_string mesh_id;
        entt::hashed_string texture_id;
    };

    class RenderQueue {
    public:
        ENGINE_INLINE RenderQueue() = default;
        ENGINE_INLINE ~RenderQueue() = default;

        RenderQueue(const RenderQueue&) = delete;
        RenderQueue& operator=(const RenderQueue&) = delete;

        ENGINE_INLINE void Initialize(engine::ChainedArena& arena, size_t max_commands);
        ENGINE_INLINE void Push(const RenderCommand& cmd);

        [[nodiscard]] ENGINE_INLINE const RenderCommand* GetCommands() const noexcept;
        [[nodiscard]] ENGINE_INLINE size_t GetCount() const noexcept;

    private:
        RenderCommand* m_commands = nullptr;
        size_t m_count = 0;
        size_t m_capacity = 0;
    };

} // namespace renderer
} // namespace engine

#include "render_queue.inl"

#endif // ENGINE_RENDERER_RENDER_QUEUE_HPP
