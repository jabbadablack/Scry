#ifndef ENGINE_CORE_INTENT_TYPES_HPP
#define ENGINE_CORE_INTENT_TYPES_HPP

#include <entt/entt.hpp>
#include <cstdint>

namespace engine {


    enum class IntentState : uint8_t {
        Pending,
        Validated,
        Rejected,
        Consumed
    };

    template <typename T>
    struct IntentHandle {
        T* data = nullptr;
        IntentState state = IntentState::Pending;
    };


} // namespace engine

#endif // ENGINE_CORE_INTENT_TYPES_HPP
