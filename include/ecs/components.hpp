#ifndef ENGINE_ECS_COMPONENTS_HPP
#define ENGINE_ECS_COMPONENTS_HPP

#include "../math/algebra.hpp"
#include <entt/entt.hpp>

namespace engine {
namespace ecs {

    struct TransformComponent {
        engine::math::Matrix4 matrix = engine::math::Matrix4::Identity();
        engine::math::Matrix4 previous_matrix = engine::math::Matrix4::Identity();
    };

    struct RenderComponent {
        entt::hashed_string mesh_id;
        entt::hashed_string texture_id;
    };

} // namespace ecs
} // namespace engine

#endif // ENGINE_ECS_COMPONENTS_HPP
