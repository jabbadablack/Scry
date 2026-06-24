#ifndef GLFW_INPUT_HPP
#define GLFW_INPUT_HPP

#include <OS/IInput.hpp>
#include <OS/types.h>
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
        bool IsKeyPressed(int key) const override;
        bool IsKeyHeld(int key) const override;
        bool IsKeyReleased(int key) const override;

    private:
        std::bitset<348> m_keys;
        std::bitset<348> m_keysPrevious;

        static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    };

} // namespace engine

#endif // GLFW_INPUT_HPP
