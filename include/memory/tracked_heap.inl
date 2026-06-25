#ifndef ENGINE_CORE_TRACKED_HEAP_INL
#define ENGINE_CORE_TRACKED_HEAP_INL

#include <new>
#include <tracy/Tracy.hpp>

namespace engine {

// Assert that all TrackedHeap allocations have been freed (call at engine shutdown)
ENGINE_INLINE void TrackedHeap::AssertNoLeaks() noexcept {
    ENGINE_ASSERT(s_current_usage.load(std::memory_order_relaxed) == 0,
                  "Memory leak detected: TrackedHeap current usage is non-zero at shutdown");
    ENGINE_ASSERT(s_total_allocated.load(std::memory_order_relaxed) == s_total_freed.load(std::memory_order_relaxed),
                  "Memory leak detected: TrackedHeap total allocated != total freed");
}

// Allocate memory with specific size and alignment tracking
ENGINE_INLINE void* TrackedHeap::Allocate(size_t size, size_t alignment) {
    ENGINE_ASSERT(size > 0, "Allocation size must be greater than zero");
    ENGINE_ASSERT(alignment > 0, "Alignment must be greater than zero");
    ENGINE_ASSERT((alignment & (alignment - 1)) == 0, "Allocation alignment must be a power of two");

    // Compute memory required for user data, structural header, and alignment padding
    size_t total_size = size + sizeof(AllocationHeader) + alignment;

    // Allocate raw block
    void* raw_ptr = ::operator new(total_size);
    ENGINE_ASSERT(raw_ptr != nullptr, "Out of memory in TrackedHeap raw allocation");

    // Align the returned user pointer address
    uintptr_t raw_address = reinterpret_cast<uintptr_t>(raw_ptr);
    uintptr_t aligned_address = (raw_address + sizeof(AllocationHeader) + (alignment - 1)) & ~(alignment - 1);

    // Store header structure immediately before the aligned user pointer
    AllocationHeader* header = reinterpret_cast<AllocationHeader*>(aligned_address - sizeof(AllocationHeader));
    header->size = size;
    header->alignment = alignment;
    header->original_ptr = raw_ptr;

    // Atomically update tracking metrics
    s_total_allocated.fetch_add(size, std::memory_order_relaxed);
    size_t current = s_current_usage.fetch_add(size, std::memory_order_relaxed) + size;

    // Compare-and-swap loop to update peak usage metric in a thread-safe way
    size_t peak = s_peak_usage.load(std::memory_order_relaxed);
    while (current > peak && !s_peak_usage.compare_exchange_weak(peak, current, std::memory_order_relaxed)) {
        // Loop executes if peak has changed concurrently
    }

    TracyAlloc(reinterpret_cast<void*>(aligned_address), size);
    return reinterpret_cast<void*>(aligned_address);
}

// Deallocate tracked pointer
ENGINE_INLINE void TrackedHeap::Deallocate(void* ptr, size_t size) noexcept {
    if (ptr == nullptr) {
        return;
    }

    // Retrieve header block from the aligned pointer
    AllocationHeader* header =
        reinterpret_cast<AllocationHeader*>(reinterpret_cast<uintptr_t>(ptr) - sizeof(AllocationHeader));

    // Verify size boundaries to catch memory leaks/faults early (if non-zero)
    if (size != 0) {
        ENGINE_ASSERT(header->size == size, "Deallocation size mismatch in TrackedHeap!");
    }
    ENGINE_ASSERT(header->original_ptr != nullptr, "Invalid metadata pointer in TrackedHeap!");

    TracyFree(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr) - sizeof(AllocationHeader)));

    size_t actual_size = header->size;
    void* original_ptr = header->original_ptr;

    // Update tracking metrics
    s_total_freed.fetch_add(actual_size, std::memory_order_relaxed);
    ENGINE_ASSERT(s_current_usage.load(std::memory_order_relaxed) >= actual_size,
                  "TrackedHeap metric underflow detected!");
    s_current_usage.fetch_sub(actual_size, std::memory_order_relaxed);

    // Free original unaligned pointer block
    ::operator delete(original_ptr);
}

// Accessors for atomic metrics
ENGINE_INLINE size_t TrackedHeap::GetTotalAllocated() noexcept {
    return s_total_allocated.load(std::memory_order_relaxed);
}

ENGINE_INLINE size_t TrackedHeap::GetTotalFreed() noexcept {
    return s_total_freed.load(std::memory_order_relaxed);
}

ENGINE_INLINE size_t TrackedHeap::GetCurrentUsage() noexcept {
    return s_current_usage.load(std::memory_order_relaxed);
}

ENGINE_INLINE size_t TrackedHeap::GetPeakUsage() noexcept {
    return s_peak_usage.load(std::memory_order_relaxed);
}

} // namespace engine

#endif // ENGINE_CORE_TRACKED_HEAP_INL
