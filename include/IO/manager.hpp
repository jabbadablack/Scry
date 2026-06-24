#ifndef ENGINE_RESOURCE_MANAGER_HPP
#define ENGINE_RESOURCE_MANAGER_HPP

#include "asset_types.h"
#include "vfs.hpp"
#include "threading/job_system.hpp"
#include "../ecs/ecs_types.hpp"
#include <entt/entt.hpp>
#include <future>
#include <vector>
#include <chrono>
#include <utility>
#include <memory>

namespace engine {
namespace io {

    // Loader wrapper to inject pre-parsed assets directly into EnTT resource caches
    template <typename T>
    struct DirectLoader {
        using result_type = std::shared_ptr<T>;
        std::shared_ptr<T> asset;

        result_type operator()() const {
            return asset;
        }
    };

    class ResourceManager {
    public:
        // TextureLoader functor supporting path loading and Custom Loader redirection
        struct TextureLoader {
            using result_type = std::shared_ptr<Texture>;
            
            [[nodiscard]] ENGINE_INLINE result_type operator()(const char* path) const;
            
            template <typename Loader>
            [[nodiscard]] ENGINE_INLINE result_type operator()(const Loader& loader) const {
                return loader();
            }
        };

        // MeshLoader functor supporting path loading and Custom Loader redirection
        struct MeshLoader {
            using result_type = std::shared_ptr<Mesh>;
            
            [[nodiscard]] ENGINE_INLINE result_type operator()(const char* path) const;
            
            template <typename Loader>
            [[nodiscard]] ENGINE_INLINE result_type operator()(const Loader& loader) const {
                return loader();
            }
        };

        // Constructor binding references
        ENGINE_INLINE explicit ResourceManager(engine::io::JobSystem& jobSystem, engine::VirtualFileSystem& vfs);
        ENGINE_INLINE ~ResourceManager() = default;

        // Rule of Delete: disable copy/move semantics (holds a reference)
        ResourceManager(const ResourceManager&) = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;
        ResourceManager(ResourceManager&&) = delete;
        ResourceManager& operator=(ResourceManager&&) = delete;

        // Asynchronous Loading APIs
        ENGINE_INLINE void SetTexture(const engine::StringHash& id, const char* path);
        ENGINE_INLINE void SetMesh(const engine::StringHash& id, const char* path);

        // Frame updates (main-thread task commit)
        ENGINE_INLINE void Update();

        // Asset loaded checks
        [[nodiscard]] ENGINE_INLINE bool IsTextureLoaded(const engine::StringHash& id) const;
        [[nodiscard]] ENGINE_INLINE bool IsMeshLoaded(const engine::StringHash& id) const;

        // Retrieval APIs
        [[nodiscard]] ENGINE_INLINE entt::resource<Texture> GetTexture(const engine::StringHash& id);
        [[nodiscard]] ENGINE_INLINE entt::resource<Mesh> GetMesh(const engine::StringHash& id);

    private:
        engine::io::JobSystem& m_jobSystem;
        engine::VirtualFileSystem& m_vfs;

        entt::resource_cache<Texture, TextureLoader> m_textures;
        entt::resource_cache<Mesh, MeshLoader> m_meshes;

        std::vector<std::pair<engine::StringHash, std::future<std::shared_ptr<Texture>>>> m_pendingTextures;
        std::vector<std::pair<engine::StringHash, std::future<std::shared_ptr<Mesh>>>> m_pendingMeshes;
    };

} // namespace io
} // namespace engine

#include "manager.inl"

#endif // ENGINE_RESOURCE_MANAGER_HPP
