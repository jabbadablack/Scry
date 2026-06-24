#ifndef ENGINE_CORE_TRACKED_HEAP_HPP
#define ENGINE_CORE_TRACKED_HEAP_HPP

#include "../OS/types.h"
#include "../debug/assert.h"
#include <atomic>
#include <cstddef>

namespace engine {


    class TrackedHeap {
    public:
        // Allocation APIs
        [[nodiscard]] ENGINE_INLINE static void* Allocate(size_t size, size_t alignment);
        ENGINE_INLINE static void Deallocate(void* ptr, size_t size) noexcept;

        // Metric queries
        [[nodiscard]] ENGINE_INLINE static size_t GetTotalAllocated() noexcept;
        [[nodiscard]] ENGINE_INLINE static size_t GetTotalFreed() noexcept;
        [[nodiscard]] ENGINE_INLINE static size_t GetCurrentUsage() noexcept;
        [[nodiscard]] ENGINE_INLINE static size_t GetPeakUsage() noexcept;

        // Debug: assert that no allocations are outstanding (call at engine shutdown)
        ENGINE_INLINE static void AssertNoLeaks() noexcept;

    private:
        struct AllocationHeader {
            size_t size;
            size_t alignment;
            void* original_ptr;
        };

        // Atomic counters for thread-safe memory tracking
        inline static std::atomic<size_t> s_total_allocated{0};
        inline static std::atomic<size_t> s_total_freed{0};
        inline static std::atomic<size_t> s_current_usage{0};
        inline static std::atomic<size_t> s_peak_usage{0};
    };


} // namespace engine

#include "tracked_heap.inl"

#endif // ENGINE_CORE_TRACKED_HEAP_HPP
