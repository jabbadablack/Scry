#ifndef ENGINE_CORE_POOL_ALLOCATOR_HPP
#define ENGINE_CORE_POOL_ALLOCATOR_HPP

#include "../OS/types.h"
#include "../debug/assert.h"
#include <cstddef>
#include <new>

namespace engine {


    class PoolAllocator {
    public:
        ENGINE_INLINE PoolAllocator(size_t chunkSize, size_t chunkCount);
        ENGINE_INLINE ~PoolAllocator();

        // Disable copy and move semantics
        PoolAllocator(const PoolAllocator&) = delete;
        PoolAllocator& operator=(const PoolAllocator&) = delete;
        PoolAllocator(PoolAllocator&&) = delete;
        PoolAllocator& operator=(PoolAllocator&&) = delete;

        [[nodiscard]] ENGINE_INLINE void* Allocate();
        ENGINE_INLINE void Free(void* ptr) noexcept;

    private:
        std::byte* m_buffer;
        void* m_freeListHead;
        size_t m_chunkSize;
        size_t m_capacity;
    };


} // namespace engine

#include "pool_allocator.inl"

#endif // ENGINE_CORE_POOL_ALLOCATOR_HPP
