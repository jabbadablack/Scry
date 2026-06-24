#ifndef ENGINE_CORE_ARRAY_INL
#define ENGINE_CORE_ARRAY_INL

namespace engine {


    // Constructor: pre-allocates from the provided ChainedArena
    template <typename T>
    ENGINE_INLINE ArenaArray<T>::ArenaArray(ChainedArena& arena, size_t capacity)
        : m_data(nullptr)
        , m_size(0)
        , m_capacity(capacity) {
        ENGINE_ASSERT(capacity > 0, "ArenaArray capacity must be greater than zero");
        
        // Allocate space for elements ensuring correct alignment
        std::byte* allocated = arena.Allocate(capacity * sizeof(T), alignof(T));
        ENGINE_ASSERT(allocated != nullptr, "Failed to allocate memory from ChainedArena for ArenaArray");
        
        m_data = reinterpret_cast<T*>(allocated);
    }

    // Destructor: calls destructors for constructed elements
    template <typename T>
    ENGINE_INLINE ArenaArray<T>::~ArenaArray() {
        // Only run the loop if elements require destruction
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_t i = 0; i < m_size; ++i) {
                m_data[i].~T();
            }
        }
        m_data = nullptr;
        m_size = 0;
        m_capacity = 0;
    }

    // Move Constructor
    template <typename T>
    ENGINE_INLINE ArenaArray<T>::ArenaArray(ArenaArray&& other) noexcept
        : m_data(other.m_data)
        , m_size(other.m_size)
        , m_capacity(other.m_capacity) {
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    // Move Assignment
    template <typename T>
    ENGINE_INLINE ArenaArray<T>& ArenaArray<T>::operator=(ArenaArray&& other) noexcept {
        ENGINE_ASSERT(this != &other, "Self-move assignment is forbidden");
        if (this != &other) {
            // Destruct current elements
            if constexpr (!std::is_trivially_destructible_v<T>) {
                for (size_t i = 0; i < m_size; ++i) {
                    m_data[i].~T();
                }
            }
            
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;

            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }
        return *this;
    }

    // PushBack Lvalue
    template <typename T>
    ENGINE_INLINE void ArenaArray<T>::PushBack(const T& value) {
        ENGINE_ASSERT(m_data != nullptr, "Cannot push to an uninitialized/moved ArenaArray");
        ENGINE_ASSERT(m_size < m_capacity, "ArenaArray capacity exhausted!");

        // Construct element in pre-allocated space using placement new
        new (&m_data[m_size]) T(value);
        m_size++;
    }

    // PushBack Rvalue
    template <typename T>
    ENGINE_INLINE void ArenaArray<T>::PushBack(T&& value) {
        ENGINE_ASSERT(m_data != nullptr, "Cannot push to an uninitialized/moved ArenaArray");
        ENGINE_ASSERT(m_size < m_capacity, "ArenaArray capacity exhausted!");

        // Construct element using move semantics and placement new
        new (&m_data[m_size]) T(std::move(value));
        m_size++;
    }

    // Clear implementation: calls destructors if needed, and resets m_size to 0
    template <typename T>
    ENGINE_INLINE void ArenaArray<T>::Clear() noexcept {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_t i = 0; i < m_size; ++i) {
                m_data[i].~T();
            }
        }
        m_size = 0;
    }

    // Begin Iterators
    template <typename T>
    ENGINE_INLINE T* ArenaArray<T>::begin() noexcept {
        return m_data;
    }

    template <typename T>
    ENGINE_INLINE const T* ArenaArray<T>::begin() const noexcept {
        return m_data;
    }

    // End Iterators
    template <typename T>
    ENGINE_INLINE T* ArenaArray<T>::end() noexcept {
        return m_data + m_size;
    }

    template <typename T>
    ENGINE_INLINE const T* ArenaArray<T>::end() const noexcept {
        return m_data + m_size;
    }

    // Raw Data Access
    template <typename T>
    ENGINE_INLINE T* ArenaArray<T>::data() noexcept {
        return m_data;
    }

    template <typename T>
    ENGINE_INLINE const T* ArenaArray<T>::data() const noexcept {
        return m_data;
    }

    // Size Queries
    template <typename T>
    ENGINE_INLINE size_t ArenaArray<T>::size() const noexcept {
        return m_size;
    }

    template <typename T>
    ENGINE_INLINE size_t ArenaArray<T>::capacity() const noexcept {
        return m_capacity;
    }

    // Element Access Operator
    template <typename T>
    ENGINE_INLINE T& ArenaArray<T>::operator[](size_t index) {
        ENGINE_ASSERT(m_data != nullptr, "Cannot index into an uninitialized/moved ArenaArray");
        ENGINE_ASSERT(index < m_size, "ArenaArray index out of bounds!");
        return m_data[index];
    }

    template <typename T>
    ENGINE_INLINE const T& ArenaArray<T>::operator[](size_t index) const {
        ENGINE_ASSERT(m_data != nullptr, "Cannot index into an uninitialized/moved ArenaArray");
        ENGINE_ASSERT(index < m_size, "ArenaArray index out of bounds!");
        return m_data[index];
    }


} // namespace engine

#endif // ENGINE_CORE_ARRAY_INL
