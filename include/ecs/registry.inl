#ifndef ENGINE_ECS_REGISTRY_INL
#define ENGINE_ECS_REGISTRY_INL

#include <type_traits>
#include <utility>

namespace engine::ecs {

    // Create an entity in the registry using custom EcsAllocator
    ENGINE_INLINE engine::ecs::Entity Registry::CreateEntity() {
        engine::ecs::Entity entity = m_registry.create();
        ENGINE_ASSERT(m_registry.valid(entity), "Failed to create entity in EnttRegistry!");
        return entity;
    }

    // Destroy an entity and all its components
    ENGINE_INLINE void Registry::DestroyEntity(engine::ecs::Entity entity) {
        ENGINE_ASSERT(m_registry.valid(entity), "Cannot destroy an invalid entity!");
        m_registry.destroy(entity);
    }

    // Add a component to an entity using EcsAllocator
    template <typename T, typename... Args>
    ENGINE_INLINE T& Registry::AddComponent(engine::ecs::Entity entity, Args&&... args) {
        ENGINE_ASSERT(m_registry.valid(entity), "Cannot add component to an invalid entity!");
        ENGINE_ASSERT(!m_registry.all_of<T>(entity), "Component already exists on this entity!");
        
        if constexpr (std::is_empty_v<T>) {
            m_registry.emplace<T>(entity, std::forward<Args>(args)...);
            thread_local static T dummy{};
            return dummy;
        } else {
            return m_registry.emplace<T>(entity, std::forward<Args>(args)...);
        }
    }

    // Remove a component from an entity
    template <typename T>
    ENGINE_INLINE void Registry::RemoveComponent(engine::ecs::Entity entity) {
        ENGINE_ASSERT(m_registry.valid(entity), "Cannot remove component from an invalid entity!");
        ENGINE_ASSERT(m_registry.all_of<T>(entity), "Component does not exist on this entity!");
        
        m_registry.remove<T>(entity);
    }

    // Retrieve a reference to a component
    template <typename T>
    ENGINE_INLINE T& Registry::GetComponent(engine::ecs::Entity entity) {
        ENGINE_ASSERT(m_registry.valid(entity), "Cannot get component from an invalid entity!");
        ENGINE_ASSERT(m_registry.all_of<T>(entity), "Component does not exist on this entity!");
        
        if constexpr (std::is_empty_v<T>) {
            thread_local static T dummy{};
            return dummy;
        } else {
            return m_registry.get<T>(entity);
        }
    }

    // Retrieve a const reference to a component
    template <typename T>
    ENGINE_INLINE const T& Registry::GetComponent(engine::ecs::Entity entity) const {
        ENGINE_ASSERT(m_registry.valid(entity), "Cannot get component from an invalid entity!");
        ENGINE_ASSERT(m_registry.all_of<T>(entity), "Component does not exist on this entity!");
        
        if constexpr (std::is_empty_v<T>) {
            thread_local static T dummy{};
            return dummy;
        } else {
            return m_registry.get<T>(entity);
        }
    }

    // Query if an entity contains a component
    template <typename T>
    ENGINE_INLINE bool Registry::HasComponent(engine::ecs::Entity entity) const {
        ENGINE_ASSERT(m_registry.valid(entity), "Cannot check component presence on an invalid entity!");
        return m_registry.all_of<T>(entity);
    }

    // Return a view over specified components
    template <typename... Components>
    ENGINE_INLINE auto Registry::View() {
        return m_registry.view<Components...>();
    }

    // Observe component creation
    template <typename T>
    ENGINE_INLINE auto Registry::ObserveCreation() {
        return entt::basic_observer<EnttRegistry>{m_registry, entt::collector.group<T>()};
    }

    // Accessors for raw EnttRegistry
    ENGINE_INLINE EnttRegistry& Registry::GetRawRegistry() noexcept {
        return m_registry;
    }

    ENGINE_INLINE const EnttRegistry& Registry::GetRawRegistry() const noexcept {
        return m_registry;
    }

} // namespace engine::ecs

#endif // ENGINE_ECS_REGISTRY_INL
