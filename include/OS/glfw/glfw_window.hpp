#ifndef GLFW_WINDOW_HPP
#define GLFW_WINDOW_HPP

#include <OS/IWindow.hpp>
#include <OS/types.h>

#if defined(ENGINE_PLATFORM_WINDOWS)
    #ifdef CreateWindow
        #undef CreateWindow
    #endif
#endif

struct GLFWwindow;

namespace engine {

    class GlfwWindow : public IWindow {
    public:
        GlfwWindow();
        ~GlfwWindow() override;

        // Disable copy semantics
        GlfwWindow(const GlfwWindow&) = delete;
        GlfwWindow& operator=(const GlfwWindow&) = delete;

        // Enable move semantics
        GlfwWindow(GlfwWindow&& other) noexcept;
        GlfwWindow& operator=(GlfwWindow&& other) noexcept;

        // Core Window API
        bool Initialize();
        bool CreateWindow(int width, int height, const char* title);
        void Shutdown();

        // Interface Overrides
        void PollEvents() override;
        void SwapBuffers() override;
        bool ShouldClose() const override;
        NativeHandles GetNativeHandles() const override;

        // Non-virtual GLFW-specific method
        GLFWwindow* GetRawWindow() const noexcept;
        bool IsWindowCreated() const noexcept;

    private:
        GLFWwindow* m_window;
        bool m_initialized;
    };

} // namespace engine

#endif // GLFW_WINDOW_HPP
