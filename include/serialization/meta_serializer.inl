#ifndef ENGINE_SERIALIZATION_META_SERIALIZER_INL
#define ENGINE_SERIALIZATION_META_SERIALIZER_INL

namespace engine::serialization {

    ENGINE_INLINE void MetaSerializer::SerializeMetaAny(IOutputArchive& archive, const char* name, const entt::meta_any& any) {
        if (!any) return;

        auto type = any.type();

        // 1. Primitive Base Cases
        if (type == entt::resolve<f32>()) { archive.WriteF32(name, any.cast<f32>()); return; }
        if (type == entt::resolve<f64>()) { archive.WriteF64(name, any.cast<f64>()); return; }
        if (type == entt::resolve<i32>()) { archive.WriteI32(name, any.cast<i32>()); return; }
        if (type == entt::resolve<u32>()) { archive.WriteU32(name, any.cast<u32>()); return; }
        if (type == entt::resolve<bool>()) { archive.WriteBool(name, any.cast<bool>()); return; }
        if (type == entt::resolve<engine::StringHash>()) { archive.WriteU32(name, any.cast<engine::StringHash>().value()); return; }

        // 2. Enum Resolution
        // meta_range yields std::pair<id_type, meta_data> — use structured binding
        if (type.is_enum()) {
            for (auto &&[eid, data] : type.data()) {
                if (data.is_const()) {
                    entt::meta_any enum_val = data.get({});
                    if (any == enum_val) {
                        if (auto prop = data.prop(engine::ecs::Hash("name"))) {
                            archive.WriteString(name, prop.value().cast<const char*>());
                            return;
                        }
                    }
                }
            }
            archive.WriteU32(name, 0); // Fallback if string resolution fails
            return;
        }

        // 3. Recursive Object Traversal
        archive.BeginObject(name);
        for (auto &&[mid, data] : type.data()) {
            const char* prop_name = "unknown";
            if (auto prop = data.prop(engine::ecs::Hash("name"))) {
                prop_name = prop.value().cast<const char*>();
            }
            entt::meta_any member_val = data.get(any);
            SerializeMetaAny(archive, prop_name, member_val);
        }
        archive.EndObject();
    }

    ENGINE_INLINE void MetaSerializer::SerializeEntity(IOutputArchive& archive, const char* name, engine::ecs::Registry& registry, engine::ecs::Entity entity) {
        archive.BeginObject(name);
        archive.WriteU32("EntityID", static_cast<u32>(entity));

        archive.BeginArray("Components", 0); // Size 0 implies dynamic array for the archive implementation

        for (auto &&[id, storage] : registry.GetRawRegistry().storage()) {
            if (storage.contains(entity)) {
                auto meta_type = entt::resolve(id);
                if (meta_type) {
                    const char* comp_name = "UnknownComponent";
                    if (auto prop = meta_type.prop(engine::ecs::Hash("name"))) {
                        comp_name = prop.value().cast<const char*>();
                    }

                    // basic_sparse_set::value(entity) wraps get_at(index(entity))
                    void* comp_ptr = storage.value(entity);
                    entt::meta_any comp_any = meta_type.from_void(comp_ptr);

                    SerializeMetaAny(archive, comp_name, comp_any);
                }
            }
        }

        archive.EndArray();
        archive.EndObject();
    }

} // namespace engine::serialization

#endif // ENGINE_SERIALIZATION_META_SERIALIZER_INL
