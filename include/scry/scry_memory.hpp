#pragma once
#include <scry/core.hpp>
#include <cstddef>
#include <cstdint>
#include <new>

namespace Scry {
namespace Memory {

// Check if a pointer is managed by the mimalloc heap.
SCRY_API bool IsUsingMimalloc(const void* ptr);

// Allocate and free memory inside the DLL to test its allocator override.
SCRY_API void* AllocInDll(size_t size);
SCRY_API void FreeInDll(void* ptr);


class SCRY_API Arena {
public:
    Arena() = default;
    ~Arena() = default;

    // Initialize the arena with backing memory and its size.
    void Init(void* backing_memory, size_t size);

    // Allocate block from the arena.
    void* Allocate(size_t size, size_t alignment = sizeof(void*));

    // Reset offset to 0.
    void Reset();

    // Stats
    size_t GetUsedMemory() const { return m_offset; }
    size_t GetTotalSize() const { return m_total_size; }
    size_t GetRemainingMemory() const { return m_total_size - m_offset; }

private:
    uint8_t* m_backing_memory = nullptr; // 8 bytes
    size_t m_total_size = 0;             // 8 bytes
    size_t m_offset = 0;                 // 8 bytes
};

template <typename T, uint32_t BlockCount>
class PoolAllocator {
public:
    PoolAllocator() = default;
    ~PoolAllocator() {
        Reset();
    }

    // Initialize with a pre-allocated contiguous memory block.
    void Init(void* memory, size_t memory_size) {
        size_t required = GetRequiredMemorySize();
        if (memory_size < required) {
            return;
        }

        size_t data_size = AlignSize(sizeof(T) * BlockCount, alignof(uint32_t));
        size_t next_free_size = AlignSize(sizeof(uint32_t) * BlockCount, alignof(int8_t));

        uint8_t* ptr = static_cast<uint8_t*>(memory);
        m_data = reinterpret_cast<T*>(ptr);
        m_next_free = reinterpret_cast<uint32_t*>(ptr + data_size);
        m_states = reinterpret_cast<int8_t*>(ptr + data_size + next_free_size);

        m_capacity = BlockCount;
        m_first_free = 0;
        m_active_count = 0;

        for (uint32_t i = 0; i < BlockCount; ++i) {
            m_states[i] = 0; // 0 = Free
            if (i < BlockCount - 1) {
                m_next_free[i] = i + 1;
            } else {
                m_next_free[i] = 0xFFFFFFFF; // INVALID_INDEX (0xFFFFFFFF)
            }
        }
    }

    // Allocate an object and return its index/handle.
    template <typename... Args>
    uint32_t Allocate(Args&&... args) {
        if (m_first_free == 0xFFFFFFFF) {
            return 0xFFFFFFFF;
        }

        uint32_t index = m_first_free;
        m_first_free = m_next_free[index];
        m_states[index] = 1; // 1 = Active
        m_active_count++;

        // Construct object in place
        new (&m_data[index]) T(static_cast<Args&&>(args)...);

        return index;
    }

    // Free an object by its index.
    void Free(uint32_t index) {
        if (index >= m_capacity || m_states[index] == 0) {
            return;
        }

        // Call destructor
        m_data[index].~T();

        m_states[index] = 0;
        m_next_free[index] = m_first_free;
        m_first_free = index;
        m_active_count--;
    }

    // Retrieve direct access pointers.
    T* Get(uint32_t index) {
        if (index >= m_capacity || m_states[index] == 0) {
            return nullptr;
        }
        return &m_data[index];
    }

    const T* Get(uint32_t index) const {
        if (index >= m_capacity || m_states[index] == 0) {
            return nullptr;
        }
        return &m_data[index];
    }

    // Reset pool and destruct active elements.
    void Reset() {
        if (m_data && m_states) {
            for (uint32_t i = 0; i < m_capacity; ++i) {
                if (m_states[i] == 1) {
                    m_data[i].~T();
                    m_states[i] = 0;
                }
            }
        }
        m_first_free = 0;
        m_active_count = 0;
        if (m_next_free) {
            for (uint32_t i = 0; i < m_capacity; ++i) {
                if (i < m_capacity - 1) {
                    m_next_free[i] = i + 1;
                } else {
                    m_next_free[i] = 0xFFFFFFFF;
                }
            }
        }
    }

    // Calculate memory size required for backing store.
    static constexpr size_t GetRequiredMemorySize() {
        size_t data_size = AlignSize(sizeof(T) * BlockCount, alignof(uint32_t));
        size_t next_free_size = AlignSize(sizeof(uint32_t) * BlockCount, alignof(int8_t));
        size_t states_size = AlignSize(sizeof(int8_t) * BlockCount, sizeof(void*));
        return data_size + next_free_size + states_size;
    }

    size_t GetCapacity() const { return m_capacity; }
    size_t GetActiveCount() const { return m_active_count; }

private:
    static constexpr size_t AlignSize(size_t size, size_t alignment) {
        return (size + (alignment - 1)) & ~(alignment - 1);
    }

    T* m_data = nullptr;                 // 8 bytes
    uint32_t* m_next_free = nullptr;     // 8 bytes
    int8_t* m_states = nullptr;          // 8 bytes
    size_t m_capacity = 0;               // 8 bytes
    uint32_t m_first_free = 0xFFFFFFFF;  // 4 bytes
    uint32_t m_active_count = 0;         // 4 bytes
};

} // namespace Memory
} // namespace Scry
