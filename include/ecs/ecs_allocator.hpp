#ifndef ENGINE_ECS_ALLOCATOR_HPP
#define ENGINE_ECS_ALLOCATOR_HPP

#include "memory/tracked_heap.hpp"
#include <cstddef>


namespace engine::ecs {

    template <typename T>
    struct EcsAllocator {
        using value_type = T;

        constexpr EcsAllocator() noexcept = default;

        // Templated copy constructor for allocator rebinding
        template <typename U>
        constexpr EcsAllocator(const EcsAllocator<U>&) noexcept {}

        // Allocate memory (NASA Rule 7: [[nodiscard]])
        [[nodiscard]] T* allocate(std::size_t n) {
            if (n == 0) {
                return nullptr;
            }
            // Ensure minimum alignment conforms to max_align_t to prevent SIMD faults
            constexpr size_t alignment = alignof(T) > alignof(std::max_align_t) ? alignof(T) : alignof(std::max_align_t);
            void* ptr = engine::TrackedHeap::Allocate(n * sizeof(T), alignment);
            return static_cast<T*>(ptr);
        }

        // Deallocate memory
        void deallocate(T* p, std::size_t n) noexcept {
            engine::TrackedHeap::Deallocate(p, n * sizeof(T));
        }
    };

    // Global comparison operators to satisfy allocator requirements
    template <typename T, typename U>
    constexpr bool operator==(const EcsAllocator<T>&, const EcsAllocator<U>&) noexcept {
        return true;
    }

    template <typename T, typename U>
    constexpr bool operator!=(const EcsAllocator<T>&, const EcsAllocator<U>&) noexcept {
        return false;
    }

} // namespace engine::ecs


#endif // ENGINE_ECS_ALLOCATOR_HPP
