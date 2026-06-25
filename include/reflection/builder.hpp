#ifndef ENGINE_REFLECTION_BUILDER_HPP
#define ENGINE_REFLECTION_BUILDER_HPP

#include "../ecs/ecs_types.hpp"
#include <entt/meta/factory.hpp>
#include <entt/meta/meta.hpp>

namespace engine {

// entt::meta_factory<T> default-constructs by asserting T is already in the
// meta_ctx (via internal::owner). Storing one as a member causes the assertion
// to fire before RegisterReflection can register T. Instead we call meta<T>()
// inline per method — it does try_emplace (idempotent) so subsequent calls
// safely retrieve the existing node without re-registering.
template <typename T>
class ReflectionBuilder {
public:
    ENGINE_INLINE explicit ReflectionBuilder(const char* class_name) {
        engine::ecs::Meta<T>()
            .type(engine::ecs::Hash(class_name))
            .prop(engine::ecs::Hash("name"), class_name);
    }

    template <auto Member>
    ENGINE_INLINE ReflectionBuilder& Field(const char* name) {
        engine::ecs::Meta<T>()
            .template data<Member>(engine::ecs::Hash(name))
            .prop(engine::ecs::Hash("name"), name);
        return *this;
    }

    template <auto Value>
    ENGINE_INLINE ReflectionBuilder& Enum(const char* name) {
        engine::ecs::Meta<T>()
            .template data<Value>(engine::ecs::Hash(name))
            .prop(engine::ecs::Hash("name"), name);
        return *this;
    }
};

} // namespace engine

#define ENGINE_REFLECT_CLASS(TYPE) engine::ReflectionBuilder<TYPE>(#TYPE)
#define ENGINE_REFLECT_FIELD(MEMBER) Field<&CurrentType::MEMBER>(#MEMBER)
#define ENGINE_REFLECT_ENUM(VALUE) Enum<CurrentType::VALUE>(#VALUE)

#endif // ENGINE_REFLECTION_BUILDER_HPP
