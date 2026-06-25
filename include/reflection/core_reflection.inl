#ifndef ENGINE_REFLECTION_CORE_REFLECTION_INL
#define ENGINE_REFLECTION_CORE_REFLECTION_INL

#include "builder.hpp"
#include "../debug/logger.hpp"
#include "../ecs/components.hpp"
#include "../ecs/ecs_types.hpp"
#include "../graphics/graphics_types.hpp"
#include "../math/math.h"
#include "../OS/keys.hpp"

#include <entt/meta/factory.hpp>
#include <entt/meta/meta.hpp>

namespace engine {

ENGINE_INLINE void RegisterCoreReflection() {
    using namespace engine::ecs;
    using namespace engine::math;
    using namespace engine::graphics;

    // ── 1. OS ENUMS ────────────────────────────────────────────────
    // Full Key registry so control configs can be serialized by name
    Meta<Key>().type(Hash("Key")).prop(Hash("name"), "Key")
        .data<Key::Unknown>(Hash("Unknown")).prop(Hash("name"), "Unknown")
        .data<Key::Space>(Hash("Space")).prop(Hash("name"), "Space")
        .data<Key::Apostrophe>(Hash("Apostrophe")).prop(Hash("name"), "Apostrophe")
        .data<Key::Comma>(Hash("Comma")).prop(Hash("name"), "Comma")
        .data<Key::Minus>(Hash("Minus")).prop(Hash("name"), "Minus")
        .data<Key::Period>(Hash("Period")).prop(Hash("name"), "Period")
        .data<Key::Slash>(Hash("Slash")).prop(Hash("name"), "Slash")
        .data<Key::Semicolon>(Hash("Semicolon")).prop(Hash("name"), "Semicolon")
        .data<Key::Equal>(Hash("Equal")).prop(Hash("name"), "Equal")
        .data<Key::A>(Hash("A")).prop(Hash("name"), "A")
        .data<Key::B>(Hash("B")).prop(Hash("name"), "B")
        .data<Key::C>(Hash("C")).prop(Hash("name"), "C")
        .data<Key::D>(Hash("D")).prop(Hash("name"), "D")
        .data<Key::E>(Hash("E")).prop(Hash("name"), "E")
        .data<Key::F>(Hash("F")).prop(Hash("name"), "F")
        .data<Key::G>(Hash("G")).prop(Hash("name"), "G")
        .data<Key::H>(Hash("H")).prop(Hash("name"), "H")
        .data<Key::I>(Hash("I")).prop(Hash("name"), "I")
        .data<Key::J>(Hash("J")).prop(Hash("name"), "J")
        .data<Key::K>(Hash("K")).prop(Hash("name"), "K")
        .data<Key::L>(Hash("L")).prop(Hash("name"), "L")
        .data<Key::M>(Hash("M")).prop(Hash("name"), "M")
        .data<Key::N>(Hash("N")).prop(Hash("name"), "N")
        .data<Key::O>(Hash("O")).prop(Hash("name"), "O")
        .data<Key::P>(Hash("P")).prop(Hash("name"), "P")
        .data<Key::Q>(Hash("Q")).prop(Hash("name"), "Q")
        .data<Key::R>(Hash("R")).prop(Hash("name"), "R")
        .data<Key::S>(Hash("S")).prop(Hash("name"), "S")
        .data<Key::T>(Hash("T")).prop(Hash("name"), "T")
        .data<Key::U>(Hash("U")).prop(Hash("name"), "U")
        .data<Key::V>(Hash("V")).prop(Hash("name"), "V")
        .data<Key::W>(Hash("W")).prop(Hash("name"), "W")
        .data<Key::X>(Hash("X")).prop(Hash("name"), "X")
        .data<Key::Y>(Hash("Y")).prop(Hash("name"), "Y")
        .data<Key::Z>(Hash("Z")).prop(Hash("name"), "Z")
        .data<Key::Num0>(Hash("Num0")).prop(Hash("name"), "Num0")
        .data<Key::Num1>(Hash("Num1")).prop(Hash("name"), "Num1")
        .data<Key::Num2>(Hash("Num2")).prop(Hash("name"), "Num2")
        .data<Key::Num3>(Hash("Num3")).prop(Hash("name"), "Num3")
        .data<Key::Num4>(Hash("Num4")).prop(Hash("name"), "Num4")
        .data<Key::Num5>(Hash("Num5")).prop(Hash("name"), "Num5")
        .data<Key::Num6>(Hash("Num6")).prop(Hash("name"), "Num6")
        .data<Key::Num7>(Hash("Num7")).prop(Hash("name"), "Num7")
        .data<Key::Num8>(Hash("Num8")).prop(Hash("name"), "Num8")
        .data<Key::Num9>(Hash("Num9")).prop(Hash("name"), "Num9")
        .data<Key::Escape>(Hash("Escape")).prop(Hash("name"), "Escape")
        .data<Key::Enter>(Hash("Enter")).prop(Hash("name"), "Enter")
        .data<Key::Tab>(Hash("Tab")).prop(Hash("name"), "Tab")
        .data<Key::Backspace>(Hash("Backspace")).prop(Hash("name"), "Backspace")
        .data<Key::Insert>(Hash("Insert")).prop(Hash("name"), "Insert")
        .data<Key::Delete>(Hash("Delete")).prop(Hash("name"), "Delete")
        .data<Key::Right>(Hash("Right")).prop(Hash("name"), "Right")
        .data<Key::Left>(Hash("Left")).prop(Hash("name"), "Left")
        .data<Key::Down>(Hash("Down")).prop(Hash("name"), "Down")
        .data<Key::Up>(Hash("Up")).prop(Hash("name"), "Up")
        .data<Key::F11>(Hash("F11")).prop(Hash("name"), "F11")
        .data<Key::LeftShift>(Hash("LeftShift")).prop(Hash("name"), "LeftShift")
        .data<Key::LeftControl>(Hash("LeftControl")).prop(Hash("name"), "LeftControl")
        .data<Key::LeftAlt>(Hash("LeftAlt")).prop(Hash("name"), "LeftAlt")
        .data<Key::RightShift>(Hash("RightShift")).prop(Hash("name"), "RightShift")
        .data<Key::RightControl>(Hash("RightControl")).prop(Hash("name"), "RightControl")
        .data<Key::RightAlt>(Hash("RightAlt")).prop(Hash("name"), "RightAlt");

    Meta<MouseButton>().type(Hash("MouseButton")).prop(Hash("name"), "MouseButton")
        .data<MouseButton::Left>(Hash("Left")).prop(Hash("name"), "Left")
        .data<MouseButton::Right>(Hash("Right")).prop(Hash("name"), "Right")
        .data<MouseButton::Middle>(Hash("Middle")).prop(Hash("name"), "Middle");

    // ── 2. MATH TYPES ──────────────────────────────────────────────────────
    // data<nullptr, getter>() silences meta_setter instantiation: a 0-arg const
    // member fn has an empty args_type, so the single-arg data<fn> path would
    // fail on type_list_element_t<0, type_list<>> inside meta_setter (EnTT v3.13).
    Meta<Vector2>().type(Hash("Vector2")).prop(Hash("name"), "Vector2")
        .data<nullptr, static_cast<const f32&(Vector2::*)() const>(&Vector2::x)>(Hash("x")).prop(Hash("name"), "x")
        .data<nullptr, static_cast<const f32&(Vector2::*)() const>(&Vector2::y)>(Hash("y")).prop(Hash("name"), "y");

    Meta<Vector3>().type(Hash("Vector3")).prop(Hash("name"), "Vector3")
        .data<nullptr, static_cast<const f32&(Vector3::*)() const>(&Vector3::x)>(Hash("x")).prop(Hash("name"), "x")
        .data<nullptr, static_cast<const f32&(Vector3::*)() const>(&Vector3::y)>(Hash("y")).prop(Hash("name"), "y")
        .data<nullptr, static_cast<const f32&(Vector3::*)() const>(&Vector3::z)>(Hash("z")).prop(Hash("name"), "z");

    Meta<Vector4>().type(Hash("Vector4")).prop(Hash("name"), "Vector4")
        .data<nullptr, static_cast<const f32&(Vector4::*)() const>(&Vector4::x)>(Hash("x")).prop(Hash("name"), "x")
        .data<nullptr, static_cast<const f32&(Vector4::*)() const>(&Vector4::y)>(Hash("y")).prop(Hash("name"), "y")
        .data<nullptr, static_cast<const f32&(Vector4::*)() const>(&Vector4::z)>(Hash("z")).prop(Hash("name"), "z")
        .data<nullptr, static_cast<const f32&(Vector4::*)() const>(&Vector4::w)>(Hash("w")).prop(Hash("name"), "w");

    // Quaternion: x/y/z/w component access for serializing rotation state
    Meta<Quaternion>().type(Hash("Quaternion")).prop(Hash("name"), "Quaternion")
        .data<nullptr, static_cast<const f32&(Quaternion::*)() const>(&Quaternion::x)>(Hash("x")).prop(Hash("name"), "x")
        .data<nullptr, static_cast<const f32&(Quaternion::*)() const>(&Quaternion::y)>(Hash("y")).prop(Hash("name"), "y")
        .data<nullptr, static_cast<const f32&(Quaternion::*)() const>(&Quaternion::z)>(Hash("z")).prop(Hash("name"), "z")
        .data<nullptr, static_cast<const f32&(Quaternion::*)() const>(&Quaternion::w)>(Hash("w")).prop(Hash("name"), "w");

    // Matrix4: const data() selects the const float* overload (two overloads exist)
    Meta<Matrix4>().type(Hash("Matrix4")).prop(Hash("name"), "Matrix4")
        .data<nullptr, static_cast<const f32*(Matrix4::*)() const>(&Matrix4::data)>(Hash("data")).prop(Hash("name"), "data");

    Meta<AABB>().type(Hash("AABB")).prop(Hash("name"), "AABB")
        .data<&AABB::min>(Hash("min")).prop(Hash("name"), "min")
        .data<&AABB::max>(Hash("max")).prop(Hash("name"), "max");

    Meta<Sphere>().type(Hash("Sphere")).prop(Hash("name"), "Sphere")
        .data<&Sphere::center>(Hash("center")).prop(Hash("name"), "center")
        .data<&Sphere::radius>(Hash("radius")).prop(Hash("name"), "radius");

    // ── 3. GRAPHICS ENUMS (Automated) ──────────────────────────────────────
    {
        using CurrentType = PrimitiveTopology;
        ENGINE_REFLECT_CLASS(PrimitiveTopology)
            .ENGINE_REFLECT_ENUM(TriangleList)
            .ENGINE_REFLECT_ENUM(LineList);
    }
    {
        using CurrentType = BufferUsage;
        ENGINE_REFLECT_CLASS(BufferUsage)
            .ENGINE_REFLECT_ENUM(Static)
            .ENGINE_REFLECT_ENUM(Dynamic);
    }
    {
        using CurrentType = RenderPass;
        ENGINE_REFLECT_CLASS(RenderPass)
            .ENGINE_REFLECT_ENUM(Opaque)
            .ENGINE_REFLECT_ENUM(Transparent)
            .ENGINE_REFLECT_ENUM(ZPrePass);
    }

    // ── 4. ECS COMPONENTS (Automated) ──────────────────────────────────────
    {
        using CurrentType = TransformComponent;
        ENGINE_REFLECT_CLASS(TransformComponent)
            .ENGINE_REFLECT_FIELD(matrix)
            .ENGINE_REFLECT_FIELD(previous_matrix);
    }
    {
        using CurrentType = RenderComponent;
        ENGINE_REFLECT_CLASS(RenderComponent)
            .ENGINE_REFLECT_FIELD(mesh_id)
            .ENGINE_REFLECT_FIELD(texture_id)
            .ENGINE_REFLECT_FIELD(topology);
    }
    {
        using CurrentType = TagComponent;
        ENGINE_REFLECT_CLASS(TagComponent)
            .ENGINE_REFLECT_FIELD(tag);
    }
    {
        using CurrentType = HierarchyComponent;
        ENGINE_REFLECT_CLASS(HierarchyComponent)
            .ENGINE_REFLECT_FIELD(parent)
            .ENGINE_REFLECT_FIELD(first_child)
            .ENGINE_REFLECT_FIELD(prev_sibling)
            .ENGINE_REFLECT_FIELD(next_sibling);
    }
    {
        using CurrentType = CameraComponent;
        ENGINE_REFLECT_CLASS(CameraComponent)
            .ENGINE_REFLECT_FIELD(fov)
            .ENGINE_REFLECT_FIELD(near_plane)
            .ENGINE_REFLECT_FIELD(far_plane)
            .ENGINE_REFLECT_FIELD(is_active)
            .ENGINE_REFLECT_FIELD(view_proj);
    }
    {
        using CurrentType = EnvironmentComponent;
        ENGINE_REFLECT_CLASS(EnvironmentComponent)
            .ENGINE_REFLECT_FIELD(bounds)
            .ENGINE_REFLECT_FIELD(gravity)
            .ENGINE_REFLECT_FIELD(ambient_color)
            .ENGINE_REFLECT_FIELD(fog_density);
    }
    {
        using CurrentType = EditorComponent;
        ENGINE_REFLECT_CLASS(EditorComponent)
            .ENGINE_REFLECT_FIELD(show_overlay);
    }

    ENGINE_LOG_INFO("Core Engine Reflection Registered");
}

} // namespace engine

#endif // ENGINE_REFLECTION_CORE_REFLECTION_INL
