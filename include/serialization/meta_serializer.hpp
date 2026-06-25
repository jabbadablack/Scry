#ifndef ENGINE_SERIALIZATION_META_SERIALIZER_HPP
#define ENGINE_SERIALIZATION_META_SERIALIZER_HPP

#include "archive.hpp"
#include "../ecs/registry.hpp"
#include "../ecs/ecs_types.hpp"

namespace engine::serialization {

    class MetaSerializer {
    public:
        // Recursively serializes any reflected C++ type into the output archive
        static ENGINE_INLINE void SerializeMetaAny(IOutputArchive& archive, const char* name, const entt::meta_any& any);

        // Iterates all components on an entity and serializes them
        static ENGINE_INLINE void SerializeEntity(IOutputArchive& archive, const char* name, engine::ecs::Registry& registry, engine::ecs::Entity entity);
    };

} // namespace engine::serialization

#include "meta_serializer.inl"

#endif // ENGINE_SERIALIZATION_META_SERIALIZER_HPP
