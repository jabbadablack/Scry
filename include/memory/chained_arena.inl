#ifndef ENGINE_MEMORY_CHAINED_ARENA_INL
#define ENGINE_MEMORY_CHAINED_ARENA_INL

namespace engine {

    ENGINE_INLINE ChainedArena::Block* ChainedArena::AllocBlock(size_t capacity) {
        size_t total = sizeof(Block) + capacity;
        void*  raw   = TrackedHeap::Allocate(total, alignof(std::max_align_t));
        ENGINE_ASSERT(raw != nullptr, "ChainedArena: TrackedHeap failed to allocate a new block");

        Block* b     = new (raw) Block{};
        b->data      = reinterpret_cast<std::byte*>(raw) + sizeof(Block);
        b->capacity  = capacity;
        b->offset.store(0, std::memory_order_relaxed);
        b->next      = nullptr;
        return b;
    }

    ENGINE_INLINE ChainedArena::ChainedArena(size_t blockSize)
        : m_blockSize(blockSize)
    {
        ENGINE_ASSERT(blockSize > 0, "ChainedArena: block size must be greater than zero");
        m_head    = AllocBlock(blockSize);
        m_current.store(m_head, std::memory_order_relaxed);
    }

    ENGINE_INLINE ChainedArena::~ChainedArena() {
        Block* b = m_head;
        while (b != nullptr) {
            Block* next  = b->next;
            size_t total = sizeof(Block) + b->capacity;
            b->~Block();
            TrackedHeap::Deallocate(b, total);
            b = next;
        }
        m_head = nullptr;
    }

    ENGINE_INLINE std::byte* ChainedArena::Allocate(size_t size, size_t alignment) {
        ENGINE_ASSERT(size > 0, "ChainedArena: allocation size must be greater than zero");
        ENGINE_ASSERT(alignment > 0, "ChainedArena: alignment must be greater than zero");
        ENGINE_ASSERT((alignment & (alignment - 1)) == 0, "ChainedArena: alignment must be a power of two");
        ENGINE_ASSERT(alignment <= alignof(std::max_align_t), "ChainedArena: alignment exceeds max_align_t");

        while (true) {
            Block* cur      = m_current.load(std::memory_order_acquire);
            size_t expected = cur->offset.load(std::memory_order_relaxed);

            while (true) {
                size_t aligned = (expected + alignment - 1) & ~(alignment - 1);
                size_t end     = aligned + size;

                if (end > cur->capacity) break;

                if (cur->offset.compare_exchange_weak(expected, end,
                        std::memory_order_release, std::memory_order_relaxed)) {
                    return cur->data + aligned;
                }
                // expected updated by CAS; retry alignment calc
            }

            // Block full — expand under lock
            std::lock_guard<std::mutex> lock(m_expandMutex);

            // Re-check: another thread may have already advanced m_current
            if (m_current.load(std::memory_order_relaxed) != cur) continue;

            if (cur->next != nullptr) {
                m_current.store(cur->next, std::memory_order_release);
                continue;
            }

            size_t  new_cap   = std::max(m_blockSize, size + alignment);
            Block*  new_block = AllocBlock(new_cap);
            cur->next = new_block;
            m_current.store(new_block, std::memory_order_release);
        }
    }

    ENGINE_INLINE void ChainedArena::Clear() noexcept {
        Block* cur = m_current.load(std::memory_order_relaxed);
        Block* b   = m_head;
        while (b != nullptr) {
            b->offset.store(0, std::memory_order_relaxed);
            if (b == cur) break;
            b = b->next;
        }
        m_current.store(m_head, std::memory_order_release);
    }

} // namespace engine

#endif // ENGINE_MEMORY_CHAINED_ARENA_INL
