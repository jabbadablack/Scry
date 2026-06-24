#ifndef ENGINE_OS_IINPUT_HPP
#define ENGINE_OS_IINPUT_HPP

#include "keys.hpp"

namespace engine {

    class IInput {
    public:
        virtual ~IInput() = default;
        virtual void Update() = 0;
        [[nodiscard]] virtual bool IsKeyPressed(Key key) const = 0;
        [[nodiscard]] virtual bool IsKeyHeld(Key key) const = 0;
        [[nodiscard]] virtual bool IsKeyReleased(Key key) const = 0;
    };

} // namespace engine

#endif // ENGINE_OS_IINPUT_HPP
