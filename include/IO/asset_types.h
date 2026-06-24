#ifndef ENGINE_RESOURCE_ASSET_TYPES_H
#define ENGINE_RESOURCE_ASSET_TYPES_H

#include "../math/algebra.hpp"
#include "../memory/tracked_heap.hpp"
#include "../ecs/ecs_allocator.hpp"
#include <vector>
#include <memory>


namespace engine::io {

    struct Texture {
        int width = 0;
        int height = 0;
        int channels = 0;
        
        // Pixel data pointer enforcing deallocation via TrackedHeap function pointer deleter
        std::unique_ptr<std::byte[], void(*)(std::byte*)> data{nullptr, [](std::byte*) {}};
    };

    struct Vertex {
        engine::math::Vector3 position;
        engine::math::Vector3 normal;
        engine::math::Vector2 uv;
    };

    struct Mesh {
        std::vector<Vertex, engine::ecs::EcsAllocator<Vertex>> vertices;
        std::vector<uint32_t, engine::ecs::EcsAllocator<uint32_t>> indices;
    };

} // namespace engine::io


#endif // ENGINE_RESOURCE_ASSET_TYPES_H
