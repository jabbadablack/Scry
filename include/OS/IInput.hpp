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

        [[nodiscard]] virtual bool IsMouseButtonPressed(MouseButton button) const = 0;
        [[nodiscard]] virtual bool IsMouseButtonHeld(MouseButton button) const = 0;
        [[nodiscard]] virtual bool IsMouseButtonReleased(MouseButton button) const = 0;
        virtual void GetMousePosition(f64& out_x, f64& out_y) const = 0;
        virtual void GetMouseDelta(f64& out_dx, f64& out_dy) const = 0;

        virtual void SetCursorVisible(bool visible) = 0;
        [[nodiscard]] virtual bool IsCursorVisible() const = 0;
        virtual void SetCursorConfined(bool confined) = 0;
        [[nodiscard]] virtual bool IsCursorConfined() const = 0;
    };

} // namespace engine

#endif // ENGINE_OS_IINPUT_HPP
