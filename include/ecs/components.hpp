#ifndef ENGINE_ECS_COMPONENTS_HPP
#define ENGINE_ECS_COMPONENTS_HPP

#include "../math/algebra.hpp"
#include "ecs_types.hpp"
#include <entt/entt.hpp>


namespace engine::ecs {

    struct TransformComponent {
        engine::math::Matrix4 matrix = engine::math::Matrix4::Identity();
        engine::math::Matrix4 previous_matrix = engine::math::Matrix4::Identity();
    };

    struct RenderComponent {
        engine::StringHash mesh_id;
        engine::StringHash texture_id;
    };

    struct TagComponent {
        engine::StringHash tag;
    };

    struct HierarchyComponent {
        engine::ecs::Entity parent       = entt::null;
        engine::ecs::Entity first_child  = entt::null;
        engine::ecs::Entity prev_sibling = entt::null;
        engine::ecs::Entity next_sibling = entt::null;
    };

    struct CameraComponent {
        f32 fov        = 45.0F;
        f32 near_plane = 0.1F;
        f32 far_plane  = 1000.0F;
        bool is_active = true;
        engine::math::Matrix4 view_proj = engine::math::Matrix4::Identity();
    };

} // namespace engine::ecs


#endif // ENGINE_ECS_COMPONENTS_HPP
