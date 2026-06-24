#ifndef ENGINE_ECS_TYPES_HPP
#define ENGINE_ECS_TYPES_HPP

#include <entt/entt.hpp>
#include <OS/types.h>

namespace engine {

    using StringHash = entt::hashed_string;

namespace ecs {

    using Entity = entt::entity;
    using IdType = entt::id_type;

    template <typename... Args>
    using View = entt::basic_view<entt::entity, entt::exclude_t<>, Args...>;

    template <typename T>
    using MetaNode = entt::meta_factory<T>;

    [[nodiscard]] ENGINE_INLINE constexpr IdType Hash(const char* str) noexcept {
        return entt::hashed_string::value(str);
    }

    template <typename T>
    [[nodiscard]] ENGINE_INLINE auto Meta() noexcept {
        return entt::meta<T>();
    }

} // namespace ecs
} // namespace engine

#endif // ENGINE_ECS_TYPES_HPP
