#ifndef ENGINE_REFLECTION_CORE_REFLECTION_INL
#define ENGINE_REFLECTION_CORE_REFLECTION_INL

#include "../debug/logger.hpp"
#include "../ecs/components.hpp"
#include "../ecs/ecs_types.hpp"
#include "../graphics/graphics_types.hpp"
#include "../math/algebra.hpp"

#include <entt/meta/factory.hpp>
#include <entt/meta/meta.hpp>

namespace engine {

ENGINE_INLINE void RegisterCoreReflection() {
    using namespace engine::ecs;
    using namespace engine::math;
    using namespace engine::graphics;

    // ── 1. GRAPHICS ENUMS ────────────────────────────────────────────────
    Meta<PrimitiveTopology>()
        .type(Hash("PrimitiveTopology"))
        .data<PrimitiveTopology::TriangleList>(Hash("TriangleList"))
        .data<PrimitiveTopology::LineList>(Hash("LineList"));

    Meta<BufferUsage>()
        .type(Hash("BufferUsage"))
        .data<BufferUsage::Static>(Hash("Static"))
        .data<BufferUsage::Dynamic>(Hash("Dynamic"));

    // ── 2. ECS COMPONENTS ────────────────────────────────────────────────
    Meta<TransformComponent>()
        .type(Hash("TransformComponent"))
        .data<&TransformComponent::matrix>(Hash("matrix"))
        .data<&TransformComponent::previous_matrix>(Hash("previous_matrix"));

    Meta<RenderComponent>()
        .type(Hash("RenderComponent"))
        .data<&RenderComponent::mesh_id>(Hash("mesh_id"))
        .data<&RenderComponent::texture_id>(Hash("texture_id"))
        .data<&RenderComponent::topology>(Hash("topology"));

    Meta<TagComponent>()
        .type(Hash("TagComponent"))
        .data<&TagComponent::tag>(Hash("tag"));

    Meta<HierarchyComponent>()
        .type(Hash("HierarchyComponent"))
        .data<&HierarchyComponent::parent>(Hash("parent"))
        .data<&HierarchyComponent::first_child>(Hash("first_child"))
        .data<&HierarchyComponent::prev_sibling>(Hash("prev_sibling"))
        .data<&HierarchyComponent::next_sibling>(Hash("next_sibling"));

    Meta<CameraComponent>()
        .type(Hash("CameraComponent"))
        .data<&CameraComponent::fov>(Hash("fov"))
        .data<&CameraComponent::near_plane>(Hash("near_plane"))
        .data<&CameraComponent::far_plane>(Hash("far_plane"))
        .data<&CameraComponent::is_active>(Hash("is_active"))
        .data<&CameraComponent::view_proj>(Hash("view_proj"));

    Meta<EnvironmentComponent>()
        .type(Hash("EnvironmentComponent"))
        .data<&EnvironmentComponent::bounds>(Hash("bounds"))
        .data<&EnvironmentComponent::gravity>(Hash("gravity"))
        .data<&EnvironmentComponent::ambient_color>(Hash("ambient_color"))
        .data<&EnvironmentComponent::fog_density>(Hash("fog_density"));

    Meta<EditorComponent>()
        .type(Hash("EditorComponent"))
        .data<&EditorComponent::show_overlay>(Hash("show_overlay"));

    ENGINE_LOG_INFO("Core Engine Reflection Registered");
}

} // namespace engine

#endif // ENGINE_REFLECTION_CORE_REFLECTION_INL
