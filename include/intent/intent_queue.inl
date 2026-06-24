#ifndef ENGINE_CORE_INTENT_QUEUE_INL
#define ENGINE_CORE_INTENT_QUEUE_INL

#include "../debug/assert.h"
#include <new>

namespace engine {


    template <typename T>
    ENGINE_INLINE void IntentQueue<T>::Initialize(ChainedArena& arena, size_t capacity) {
        ENGINE_ASSERT(capacity > 0, "IntentQueue capacity must be greater than zero");

        m_capacity = capacity;
        m_count.store(0, std::memory_order_relaxed);

        std::byte* bytes = arena.Allocate(capacity * sizeof(IntentHandle<T>), alignof(IntentHandle<T>));
        ENGINE_ASSERT(bytes != nullptr, "IntentQueue failed to allocate buffer from ChainedArena");
        m_buffer = reinterpret_cast<IntentHandle<T>*>(bytes);
        for (size_t i = 0; i < capacity; ++i) {
            new (&m_buffer[i]) IntentHandle<T>();
        }
    }

    template <typename T>
    ENGINE_INLINE IntentHandle<T>* IntentQueue<T>::Push(const T& payload, ChainedArena& arena) {
        ENGINE_ASSERT(m_buffer != nullptr, "IntentQueue::Push called before Initialize");
        size_t index = m_count.fetch_add(1, std::memory_order_relaxed);
        ENGINE_ASSERT(index < m_capacity, "IntentQueue capacity exceeded!");

        std::byte* payload_bytes = arena.Allocate(sizeof(T), alignof(T));
        T* allocated_payload = new (payload_bytes) T(payload);

        m_buffer[index].data = allocated_payload;
        m_buffer[index].state = IntentState::Pending;

        return &m_buffer[index];
    }


} // namespace engine

#endif // ENGINE_CORE_INTENT_QUEUE_INL
