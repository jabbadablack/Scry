#include <doctest/doctest.h>
#include <memory/tracked_heap.hpp>
#include <glfw/glfw_window.hpp>
#include <IO/manager.hpp>

// Mock loader that allocates tracked memory directly to prevent disk operations in CI tests
struct MockTextureLoader {
    using result_type = std::shared_ptr<engine::io::Texture>;

    result_type operator()(int width, int height, int channels) const {
        size_t total_size = static_cast<size_t>(width) * height * channels;
        void* raw_mem = engine::TrackedHeap::Allocate(total_size, 16);
        
        auto texture = std::make_shared<engine::io::Texture>();
        texture->width = width;
        texture->height = height;
        texture->channels = channels;
        
        // Match the updated unique_ptr type and decay lambda to C function pointer
        texture->data = std::unique_ptr<std::byte[], void(*)(std::byte*)>(
            reinterpret_cast<std::byte*>(raw_mem),
            [](std::byte* p) {
                if (p != nullptr) {
                    struct Header {
                        size_t size;
                        size_t alignment;
                        void* original_ptr;
                    };
                    Header* header = reinterpret_cast<Header*>(reinterpret_cast<uintptr_t>(p) - sizeof(Header));
                    size_t size = header->size;
                    engine::TrackedHeap::Deallocate(p, size);
                }
            }
        );

        return texture;
    }
};

TEST_CASE("ResourceManager Caching and Memory Tracking") {
    // Explicitly template the cache on Type and MockTextureLoader
    entt::resource_cache<engine::io::Texture, MockTextureLoader> texture_cache;
    auto id = entt::hashed_string{"test_texture_id"};

    SUBCASE("Load and cache hit memory verification") {
        size_t initial_usage = engine::TrackedHeap::GetCurrentUsage();

        // Load the texture for the first time
        auto [it1, inserted1] = texture_cache.load(id.value(), 64, 64, 4);
        REQUIRE(it1 != texture_cache.end());
        REQUIRE(inserted1 == true);
        
        auto res1 = texture_cache[id.value()];
        REQUIRE(res1);
        
        size_t post_load_usage = engine::TrackedHeap::GetCurrentUsage();
        REQUIRE(post_load_usage > initial_usage); // Verify memory increased

        // Request the same texture - should hit cache
        auto [it2, inserted2] = texture_cache.load(id.value(), 64, 64, 4);
        REQUIRE(it2 != texture_cache.end());
        REQUIRE(inserted2 == false); // Verify it was NOT inserted again
        
        auto res2 = texture_cache[id.value()];
        REQUIRE(res2);
        
        size_t post_cache_usage = engine::TrackedHeap::GetCurrentUsage();
        REQUIRE(post_cache_usage == post_load_usage); // Verify no double allocation

        // Verify that both handles point to the exact same texture data pointer
        REQUIRE(res1->data.get() == res2->data.get());
        REQUIRE(res1->width == res2->width);
        REQUIRE(res1->height == res2->height);
    }
}

TEST_CASE("ResourceManager Asynchronous Pipeline") {
    engine::GlfwWindow platform;
    REQUIRE(platform.Initialize() == true);

    {
        engine::io::JobSystem jobSystem;
        engine::VirtualFileSystem vfs;
        engine::io::ResourceManager manager(jobSystem, vfs);
        auto mesh_id = entt::hashed_string{"test_async_mesh"};

        // Submit asynchronous load task using the mock prefix to bypass filesystem
        manager.LoadMeshAsync(mesh_id, "mock://test_mesh_file.obj");

        // Wait for all Taskflow background threads to complete
        jobSystem.GetExecutor().wait_for_all();

        // Verify that the mesh is NOT yet committed to EnTT cache before Update()
        REQUIRE_FALSE(manager.IsMeshLoaded(mesh_id));

        // Update to poll and commit the completed future
        manager.Update();

        // Verify that the mesh is now committed to cache
        REQUIRE(manager.IsMeshLoaded(mesh_id));

        auto mesh = manager.GetMesh(mesh_id);
        REQUIRE(mesh);
        REQUIRE(mesh->vertices.size() == 1);
        REQUIRE(mesh->indices.size() == 3);
    }

    platform.Shutdown();
}
