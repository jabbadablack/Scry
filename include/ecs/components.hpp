#ifndef ENGINE_ECS_COMPONENTS_HPP
#define ENGINE_ECS_COMPONENTS_HPP

#include "../math/algebra.hpp"
#include "ecs_types.hpp"
#include <entt/entt.hpp>


#include "../graphics/graphics_types.hpp"
#include "../math/geometry.hpp"

namespace engine::ecs {

    struct TransformComponent {
        engine::math::Matrix4 matrix = engine::math::Matrix4::Identity();
        engine::math::Matrix4 previous_matrix = engine::math::Matrix4::Identity();
    };

    struct RenderComponent {
        engine::StringHash mesh_id;
        engine::StringHash texture_id;
        engine::graphics::PrimitiveTopology topology = engine::graphics::PrimitiveTopology::TriangleList;
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

    struct EnvironmentComponent {
        engine::math::AABB    bounds        = { engine::math::Vector3(-10000.0F, -10000.0F, -10000.0F), engine::math::Vector3(10000.0F, 10000.0F, 10000.0F) };
        engine::math::Vector3 gravity       = engine::math::Vector3(0.0F, -9.81F, 0.0F);
        engine::math::Vector4 ambient_color = engine::math::Vector4(0.1F, 0.1F, 0.1F, 1.0F);
        f32                   fog_density   = 0.01F;
    };

} // namespace engine::ecs


#endif // ENGINE_ECS_COMPONENTS_HPP
