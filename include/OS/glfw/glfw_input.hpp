#ifndef GLFW_INPUT_HPP
#define GLFW_INPUT_HPP

#include <OS/IInput.hpp>
#include <OS/types.h>
#include "../keys.hpp"
#include <bitset>

struct GLFWwindow;

namespace engine {

    class GlfwInput : public IInput {
    public:
        GlfwInput() = default;
        ~GlfwInput() override = default;

        // Keep initialize to bind with raw GLFW window
        void Initialize(GLFWwindow* window);

        // Interface Overrides
        void Update() override;
        [[nodiscard]] bool IsKeyPressed(Key key) const override;
        [[nodiscard]] bool IsKeyHeld(Key key) const override;
        [[nodiscard]] bool IsKeyReleased(Key key) const override;

        [[nodiscard]] bool IsMouseButtonPressed(MouseButton button) const override;
        [[nodiscard]] bool IsMouseButtonHeld(MouseButton button) const override;
        [[nodiscard]] bool IsMouseButtonReleased(MouseButton button) const override;
        void GetMousePosition(f64& out_x, f64& out_y) const override;
        void GetMouseDelta(f64& out_dx, f64& out_dy) const override;

        void SetCursorVisible(bool visible) override;
        [[nodiscard]] bool IsCursorVisible() const override;
        void SetCursorConfined(bool confined) override;
        [[nodiscard]] bool IsCursorConfined() const override;

    private:
        GLFWwindow* m_window = nullptr;
        bool m_cursorVisible = true;
        bool m_cursorConfined = false;
        void ApplyCursorState();

        std::bitset<348> m_keys;
        std::bitset<348> m_keysPrevious;

        std::bitset<8> m_mouse;
        std::bitset<8> m_mousePrevious;
        f64 m_mouseX = 0.0;
        f64 m_mouseY = 0.0;
        f64 m_mouseDeltaX = 0.0;
        f64 m_mouseDeltaY = 0.0;

        static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
        static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    };

} // namespace engine

#endif // GLFW_INPUT_HPP
