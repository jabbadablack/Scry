#ifndef ENGINE_ECS_TYPES_HPP
#define ENGINE_ECS_TYPES_HPP

#include <entt/entt.hpp>

namespace engine {

    using StringHash = entt::hashed_string;

namespace ecs {

    using Entity = entt::entity;

    template <typename... Args>
    using View = entt::basic_view<entt::entity, entt::exclude_t<>, Args...>;

    template <typename T>
    using MetaNode = entt::meta_factory<T>;

} // namespace ecs
} // namespace engine

#endif // ENGINE_ECS_TYPES_HPP
