#ifndef ENGINE_RESOURCE_MANAGER_INL
#define ENGINE_RESOURCE_MANAGER_INL

#include <stb_image.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <filesystem>
#include <string>
#include <string_view>
#include "../debug/logger.hpp"

namespace engine::io {

    // TextureLoader: reads from a VFS memory buffer via stbi_load_from_memory
    ENGINE_INLINE ResourceManager::TextureLoader::result_type
    ResourceManager::TextureLoader::operator()(const std::vector<std::byte, engine::ecs::EcsAllocator<std::byte>>& buffer) const {
        ENGINE_ASSERT(!buffer.empty(), "Texture buffer cannot be empty!");

        int width = 0;
        int height = 0;
        int channels = 0;

        stbi_uc* pixels = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(buffer.data()),
            static_cast<int>(buffer.size()),
            &width, &height, &channels, 0
        );
        if (pixels == nullptr) {
            ENGINE_ASSERT(false, "Failed to decode texture from memory buffer!");
            return nullptr;
        }

        auto texture = std::make_shared<Texture>();
        texture->width    = width;
        texture->height   = height;
        texture->channels = channels;

        texture->data = std::unique_ptr<std::byte[], void(*)(std::byte*)>(
            reinterpret_cast<std::byte*>(pixels),
            [](std::byte* p) {
                if (p != nullptr) {
                    stbi_image_free(p);
                }
            }
        );

        return texture;
    }

    // MeshLoader: reads from a VFS memory buffer via ReadFileFromMemory
    ENGINE_INLINE ResourceManager::MeshLoader::result_type
    ResourceManager::MeshLoader::operator()(const std::vector<std::byte, engine::ecs::EcsAllocator<std::byte>>& buffer, const char* extension) const {
        ENGINE_ASSERT(!buffer.empty(), "Mesh buffer cannot be empty!");
        ENGINE_ASSERT(extension != nullptr, "Mesh extension cannot be null!");

        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFileFromMemory(
            buffer.data(),
            buffer.size(),
            aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals,
            extension
        );

        if (scene == nullptr || scene->mNumMeshes == 0) {
            ENGINE_ASSERT(false, "Failed to parse mesh from memory buffer via Assimp!");
            return nullptr;
        }

        auto mesh = std::make_shared<Mesh>();
        const aiMesh* ai_mesh = scene->mMeshes[0];

        mesh->vertices.reserve(ai_mesh->mNumVertices);
        for (unsigned int i = 0; i < ai_mesh->mNumVertices; ++i) {
            Vertex vertex;

            vertex.position.x() = ai_mesh->mVertices[i].x;
            vertex.position.y() = ai_mesh->mVertices[i].y;
            vertex.position.z() = ai_mesh->mVertices[i].z;

            if (ai_mesh->HasNormals()) {
                vertex.normal.x() = ai_mesh->mNormals[i].x;
                vertex.normal.y() = ai_mesh->mNormals[i].y;
                vertex.normal.z() = ai_mesh->mNormals[i].z;
            } else {
                vertex.normal.setZero();
            }

            if (ai_mesh->HasTextureCoords(0)) {
                vertex.uv.x() = ai_mesh->mTextureCoords[0][i].x;
                vertex.uv.y() = ai_mesh->mTextureCoords[0][i].y;
            } else {
                vertex.uv.setZero();
            }

            mesh->vertices.push_back(vertex);
        }

        mesh->indices.reserve(ai_mesh->mNumFaces * 3);
        for (unsigned int i = 0; i < ai_mesh->mNumFaces; ++i) {
            const aiFace& face = ai_mesh->mFaces[i];
            ENGINE_ASSERT(face.mNumIndices == 3, "Mesh face is not a triangle!");
            mesh->indices.push_back(face.mIndices[0]);
            mesh->indices.push_back(face.mIndices[1]);
            mesh->indices.push_back(face.mIndices[2]);
        }

        return mesh;
    }

    ENGINE_INLINE ResourceManager::ResourceManager(engine::io::JobSystem& jobSystem, engine::VirtualFileSystem& vfs)
        : m_jobSystem(jobSystem), m_vfs(vfs) {
        ENGINE_ASSERT(jobSystem.GetExecutor().num_workers() > 0, "ResourceManager requires a live JobSystem executor");
        ENGINE_LOG_INFO("ResourceManager initialized");
    }

    ENGINE_INLINE void ResourceManager::SetTexture(const engine::StringHash& id, const char* path) {
        ENGINE_ASSERT(path != nullptr, "Texture path cannot be null");
        ENGINE_ASSERT(id.value() != 0, "Texture id hash cannot be zero");

        if (m_textures.contains(id.value())) {
            return;
        }

        ENGINE_LOG_INFO("Loading texture async: " + std::string(path));

        std::string path_str(path);
        auto future = m_jobSystem.RunTask([&vfs = m_vfs, path_str]() -> std::shared_ptr<Texture> {
            std::vector<std::byte, engine::ecs::EcsAllocator<std::byte>> buffer;
            if (!vfs.ReadFile(path_str, buffer)) {
                ENGINE_LOG_ERROR("VFS failed to read texture: " + path_str);
                return nullptr;
            }
            TextureLoader loader;
            return loader(buffer);
        });

        m_pendingTextures.emplace_back(id, std::move(future));
    }

    ENGINE_INLINE void ResourceManager::SetMesh(const engine::StringHash& id, const char* path) {
        ENGINE_ASSERT(path != nullptr, "Mesh path cannot be null");
        ENGINE_ASSERT(id.value() != 0, "Mesh id hash cannot be zero");

        if (m_meshes.contains(id.value())) {
            return;
        }

        ENGINE_LOG_INFO("Loading mesh async: " + std::string(path));

        std::string path_str(path);
        auto future = m_jobSystem.RunTask([&vfs = m_vfs, path_str]() -> std::shared_ptr<Mesh> {
            std::vector<std::byte, engine::ecs::EcsAllocator<std::byte>> buffer;
            if (!vfs.ReadFile(path_str, buffer)) {
                ENGINE_LOG_ERROR("VFS failed to read mesh: " + path_str);
                return nullptr;
            }
            std::string ext = std::filesystem::path(path_str).extension().string();
            if (!ext.empty() && ext[0] == '.') {
                ext = ext.substr(1);
            }
            MeshLoader loader;
            return loader(buffer, ext.c_str());
        });

        m_pendingMeshes.emplace_back(id, std::move(future));
    }

    ENGINE_INLINE void ResourceManager::Update() {
        for (int i = static_cast<int>(m_pendingTextures.size()) - 1; i >= 0; --i) {
            auto& [id, future] = m_pendingTextures[i];
            if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                std::shared_ptr<Texture> texture = future.get();
                ENGINE_ASSERT(texture != nullptr, "Asynchronously loaded texture is null!");
                m_textures.load<DirectLoader<Texture>>(id.value(), DirectLoader<Texture>{texture});
                ENGINE_LOG_INFO("Texture loaded: " + std::string(id.data()));
                m_pendingTextures.erase(m_pendingTextures.begin() + i);
            }
        }

        for (int i = static_cast<int>(m_pendingMeshes.size()) - 1; i >= 0; --i) {
            auto& [id, future] = m_pendingMeshes[i];
            if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                std::shared_ptr<Mesh> mesh = future.get();
                ENGINE_ASSERT(mesh != nullptr, "Asynchronously loaded mesh is null!");
                m_meshes.load<DirectLoader<Mesh>>(id.value(), DirectLoader<Mesh>{mesh});
                ENGINE_LOG_INFO("Mesh loaded: " + std::string(id.data()));
                m_pendingMeshes.erase(m_pendingMeshes.begin() + i);
            }
        }
    }

    ENGINE_INLINE entt::resource<Texture> ResourceManager::GetTexture(const engine::StringHash& id) {
        ENGINE_ASSERT(id.value() != 0, "GetTexture called with zero hash id");
        ENGINE_ASSERT(IsTextureLoaded(id), "GetTexture called for a texture that has not finished loading");
        return m_textures[id.value()];
    }

    ENGINE_INLINE entt::resource<Mesh> ResourceManager::GetMesh(const engine::StringHash& id) {
        ENGINE_ASSERT(id.value() != 0, "GetMesh called with zero hash id");
        ENGINE_ASSERT(IsMeshLoaded(id), "GetMesh called for a mesh that has not finished loading");
        return m_meshes[id.value()];
    }

    ENGINE_INLINE bool ResourceManager::IsTextureLoaded(const engine::StringHash& id) const {
        ENGINE_ASSERT(id.value() != 0, "IsTextureLoaded called with zero hash id");
        return m_textures.contains(id.value());
    }

    ENGINE_INLINE bool ResourceManager::IsMeshLoaded(const engine::StringHash& id) const {
        ENGINE_ASSERT(id.value() != 0, "IsMeshLoaded called with zero hash id");
        return m_meshes.contains(id.value());
    }

} // namespace engine::io

#endif // ENGINE_RESOURCE_MANAGER_INL
