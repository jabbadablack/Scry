#ifndef ENGINE_ECS_REGISTRY_HPP
#define ENGINE_ECS_REGISTRY_HPP

#include "OS/types.h"
#include "debug/assert.h"
#include "ecs_allocator.hpp"
#include <entt/entt.hpp>

namespace engine {
namespace ecs {

    // Override the default EnTT registry to use our thread-safe custom allocator
    using EnttRegistry = entt::basic_registry<entt::entity, engine::ecs::EcsAllocator<entt::entity>>;

    class Registry {
    public:
        // Constructor & Destructor
        ENGINE_INLINE Registry() = default;
        ENGINE_INLINE ~Registry() = default;

        // Disable copy semantics
        Registry(const Registry&) = delete;
        Registry& operator=(const Registry&) = delete;

        // Enable move semantics
        ENGINE_INLINE Registry(Registry&& other) noexcept = default;
        ENGINE_INLINE Registry& operator=(Registry&& other) noexcept = default;

        // Entity APIs
        [[nodiscard]] ENGINE_INLINE entt::entity CreateEntity();
        ENGINE_INLINE void DestroyEntity(entt::entity entity);

        // Component CRUD operations
        template <typename T, typename... Args>
        ENGINE_INLINE T& AddComponent(entt::entity entity, Args&&... args);

        template <typename T>
        ENGINE_INLINE void RemoveComponent(entt::entity entity);

        template <typename T>
        [[nodiscard]] ENGINE_INLINE T& GetComponent(entt::entity entity);

        template <typename T>
        [[nodiscard]] ENGINE_INLINE const T& GetComponent(entt::entity entity) const;

        template <typename T>
        [[nodiscard]] ENGINE_INLINE bool HasComponent(entt::entity entity) const;

        // Multi-component querying
        template <typename... Components>
        [[nodiscard]] ENGINE_INLINE auto View();

        // Reactive signals
        template <typename T>
        [[nodiscard]] ENGINE_INLINE auto ObserveCreation();

        // EnTT registry accessors
        [[nodiscard]] ENGINE_INLINE EnttRegistry& GetRawRegistry() noexcept;
        [[nodiscard]] ENGINE_INLINE const EnttRegistry& GetRawRegistry() const noexcept;

    private:
        EnttRegistry m_registry;
    };

} // namespace ecs
} // namespace engine

// Include inline implementations
#include "registry.inl"

#endif // ENGINE_ECS_REGISTRY_HPP
