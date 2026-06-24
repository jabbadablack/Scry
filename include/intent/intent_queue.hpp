#ifndef ENGINE_CORE_INTENT_QUEUE_HPP
#define ENGINE_CORE_INTENT_QUEUE_HPP

#include "../memory/chained_arena.hpp"
#include "intent_types.hpp"
#include <atomic>

namespace engine {


    template <typename T>
    class IntentQueue {
    public:
        IntentQueue() = default;
        ~IntentQueue() = default;

        // Delete copy and move semantics
        IntentQueue(const IntentQueue&) = delete;
        IntentQueue& operator=(const IntentQueue&) = delete;
        IntentQueue(IntentQueue&&) = delete;
        IntentQueue& operator=(IntentQueue&&) = delete;

        ENGINE_INLINE void Initialize(ChainedArena& arena, size_t capacity);
        ENGINE_INLINE IntentHandle<T>* Push(const T& payload, ChainedArena& arena);

        [[nodiscard]] ENGINE_INLINE IntentHandle<T>* begin() noexcept {
            return m_buffer;
        }

        [[nodiscard]] ENGINE_INLINE IntentHandle<T>* end() noexcept {
            return m_buffer + m_count.load(std::memory_order_relaxed);
        }

        [[nodiscard]] ENGINE_INLINE const IntentHandle<T>* begin() const noexcept {
            return m_buffer;
        }

        [[nodiscard]] ENGINE_INLINE const IntentHandle<T>* end() const noexcept {
            return m_buffer + m_count.load(std::memory_order_relaxed);
        }

    private:
        IntentHandle<T>* m_buffer = nullptr;
        size_t m_capacity = 0;
        std::atomic<size_t> m_count{0};
    };


} // namespace engine

#include "intent_queue.inl"

#endif // ENGINE_CORE_INTENT_QUEUE_HPP
