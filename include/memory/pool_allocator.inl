#ifndef ENGINE_CORE_POOL_ALLOCATOR_INL
#define ENGINE_CORE_POOL_ALLOCATOR_INL

namespace engine {


    ENGINE_INLINE PoolAllocator::PoolAllocator(size_t chunkSize, size_t chunkCount)
        : m_buffer(nullptr)
        , m_freeListHead(nullptr)
        , m_chunkSize(chunkSize < sizeof(void*) ? sizeof(void*) : chunkSize)
        , m_capacity(m_chunkSize * chunkCount) {
        ENGINE_ASSERT(chunkSize > 0, "Chunk size must be greater than zero");
        ENGINE_ASSERT(chunkCount > 0, "Chunk count must be greater than zero");

        m_buffer = static_cast<std::byte*>(::operator new(m_capacity));
        ENGINE_ASSERT(m_buffer != nullptr, "Failed to allocate backing buffer for PoolAllocator");

        // Set up the free list
        m_freeListHead = m_buffer;
        std::byte* current = m_buffer;
        for (size_t i = 0; i < chunkCount - 1; ++i) {
            std::byte* next = current + m_chunkSize;
            *reinterpret_cast<void**>(current) = next;
            current = next;
        }
        *reinterpret_cast<void**>(current) = nullptr;
    }

    ENGINE_INLINE PoolAllocator::~PoolAllocator() {
        ::operator delete(m_buffer);
        m_buffer = nullptr;
        m_freeListHead = nullptr;
        m_chunkSize = 0;
        m_capacity = 0;
    }

    ENGINE_INLINE void* PoolAllocator::Allocate() {
        if (m_freeListHead == nullptr) {
            ENGINE_ASSERT(false, "PoolAllocator: Out of memory / Pool exhaustion!");
            return nullptr;
        }

        void* chunk = m_freeListHead;
        m_freeListHead = *reinterpret_cast<void**>(chunk);
        return chunk;
    }

    ENGINE_INLINE void PoolAllocator::Free(void* ptr) noexcept {
        if (ptr == nullptr) {
            return;
        }
        *reinterpret_cast<void**>(ptr) = m_freeListHead;
        m_freeListHead = ptr;
    }


} // namespace engine

#endif // ENGINE_CORE_POOL_ALLOCATOR_INL
