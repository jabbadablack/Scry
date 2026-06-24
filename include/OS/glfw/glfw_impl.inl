#include "glfw_window.hpp"
#include "glfw_input.hpp"
#include <debug/logger.hpp>
#include <debug/assert.h>

#include <stdexcept>
#include <utility>

#include <GLFW/glfw3.h>
#if defined(ENGINE_PLATFORM_WINDOWS)
    #define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(ENGINE_PLATFORM_LINUX)
    #define GLFW_EXPOSE_NATIVE_X11
    #define GLFW_EXPOSE_NATIVE_WAYLAND
#endif
#include <GLFW/glfw3native.h>

#if defined(ENGINE_PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <timeapi.h>
    #pragma comment(lib, "winmm.lib")

    #ifdef CreateWindow
        #undef CreateWindow
    #endif
#endif

namespace engine {

#if defined(ENGINE_PLATFORM_WINDOWS)
    static void EnableWindowsConsoleColors() {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);
            }
        }
    }
#endif

    // GlfwWindow implementations
    GlfwWindow::GlfwWindow()
        : m_window(nullptr)
        , m_initialized(false) {}

    GlfwWindow::~GlfwWindow() {
        Shutdown();
    }

    GlfwWindow::GlfwWindow(GlfwWindow&& other) noexcept
        : m_window(other.m_window)
        , m_initialized(other.m_initialized) {
        other.m_window = nullptr;
        other.m_initialized = false;
    }

    GlfwWindow& GlfwWindow::operator=(GlfwWindow&& other) noexcept {
        ENGINE_ASSERT(this != &other, "Self-move assignment on GlfwWindow is forbidden");
        if (this != &other) {
            Shutdown();
            m_window = other.m_window;
            m_initialized = other.m_initialized;
            other.m_window = nullptr;
            other.m_initialized = false;
        }
        return *this;
    }

    bool GlfwWindow::Initialize() {
        ENGINE_ASSERT(m_window == nullptr, "GlfwWindow::Initialize called with a live window handle -- call Shutdown first");

        if (m_initialized) {
            return true;
        }

#if defined(ENGINE_PLATFORM_WINDOWS)
        EnableWindowsConsoleColors();
        timeBeginPeriod(1);
#endif

        glfwSetErrorCallback([](int error, const char* description) {
            ENGINE_LOG_ERROR("[GLFW] (" + std::to_string(error) + "): " + description);
        });

        if (!glfwInit()) {
            ENGINE_LOG_ERROR("GlfwWindow: failed to initialize GLFW");
            return false;
        }

        m_initialized = true;
        ENGINE_LOG_INFO("GlfwWindow: GLFW initialized");
        return true;
    }

    bool GlfwWindow::CreateWindow(int width, int height, const char* title) {
        ENGINE_ASSERT(m_initialized, "GlfwWindow::CreateWindow called before Initialize");
        ENGINE_ASSERT(width > 0, "Window width must be positive");
        ENGINE_ASSERT(height > 0, "Window height must be positive");
        ENGINE_ASSERT(title != nullptr, "Window title cannot be null");

        if (m_window) {
            ENGINE_LOG_WARN("GlfwWindow: a window already exists -- destroying it before creating a new one");
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (!m_window) {
            ENGINE_LOG_ERROR("GlfwWindow: failed to create GLFW window");
            return false;
        }

        ENGINE_LOG_INFO("GlfwWindow: window created (" + std::to_string(width) + "x" + std::to_string(height) + ") '" + title + "'");
        return true;
    }

    void GlfwWindow::PollEvents() {
        ENGINE_ASSERT(m_initialized, "GlfwWindow::PollEvents called before Initialize");
        ENGINE_ASSERT(m_window != nullptr, "GlfwWindow::PollEvents called with no window");

        glfwPollEvents();
    }

    void GlfwWindow::SwapBuffers() {
        ENGINE_ASSERT(m_initialized, "GlfwWindow::SwapBuffers called before Initialize");
        ENGINE_ASSERT(m_window != nullptr, "GlfwWindow::SwapBuffers called with no window");
        // No-op for Vulkan/bgfx (handled by the graphics API swapchain presentation)
    }

    bool GlfwWindow::ShouldClose() const {
        ENGINE_ASSERT(m_initialized, "GlfwWindow::ShouldClose called before Initialize");
        ENGINE_ASSERT(m_window != nullptr, "GlfwWindow::ShouldClose called with no window");

        return glfwWindowShouldClose(m_window) != 0;
    }

    void GlfwWindow::Shutdown() {
        ENGINE_ASSERT(m_initialized || m_window == nullptr, "GlfwWindow: window handle present without GLFW initialization");

        if (m_window) {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
            ENGINE_LOG_INFO("GlfwWindow: window destroyed");
        }

        if (m_initialized) {
            glfwTerminate();
#if defined(ENGINE_PLATFORM_WINDOWS)
            timeEndPeriod(1);
#endif
            m_initialized = false;
            ENGINE_LOG_INFO("GlfwWindow: GLFW terminated");
        }
    }

    GLFWwindow* GlfwWindow::GetRawWindow() const noexcept {
        ENGINE_ASSERT(m_initialized, "GetRawWindow called before Initialize");
        ENGINE_ASSERT(m_window != nullptr, "GetRawWindow called with no window created");
        return m_window;
    }

    bool GlfwWindow::IsWindowCreated() const noexcept {
        return m_window != nullptr;
    }

    NativeHandles GlfwWindow::GetNativeHandles() const {
        ENGINE_ASSERT(m_initialized, "GlfwWindow::GetNativeHandles called before Initialize");
        ENGINE_ASSERT(m_window != nullptr, "GlfwWindow::GetNativeHandles called with no window");

        NativeHandles handles{.window=nullptr, .display=nullptr};
#if defined(ENGINE_PLATFORM_WINDOWS)
        handles.window = glfwGetWin32Window(m_window);
#elif defined(ENGINE_PLATFORM_LINUX)
        handles.window = reinterpret_cast<void*>(glfwGetX1Window(m_window));
        handles.display = glfwGetX1Display();
#endif
        return handles;
    }

    // GlfwInput implementations
    void GlfwInput::Initialize(GLFWwindow* window) {
        ENGINE_ASSERT(window != nullptr, "GlfwInput::Initialize called with a null GLFW window");

        glfwSetWindowUserPointer(window, this);
        glfwSetKeyCallback(window, KeyCallback);

        ENGINE_LOG_INFO("GlfwInput: input initialized");
    }

    void GlfwInput::Update() {
        ENGINE_ASSERT(m_keys.size() == 348, "GlfwInput: key bitset size invariant broken");
        ENGINE_ASSERT(m_keysPrevious.size() == 348, "GlfwInput: previous key bitset size invariant broken");

        m_keysPrevious = m_keys;
    }

    bool GlfwInput::IsKeyPressed(Key key) const {
        i32 k = static_cast<i32>(key);
        ENGINE_ASSERT(k >= 0 && k < 348, "Key index out of range");

        return m_keys.test(k) && !m_keysPrevious.test(k);
    }

    bool GlfwInput::IsKeyHeld(Key key) const {
        i32 k = static_cast<i32>(key);
        ENGINE_ASSERT(k >= 0 && k < 348, "Key index out of range");

        return m_keys.test(k);
    }

    bool GlfwInput::IsKeyReleased(Key key) const {
        i32 k = static_cast<i32>(key);
        ENGINE_ASSERT(k >= 0 && k < 348, "Key index out of range");

        return !m_keys.test(k) && m_keysPrevious.test(k);
    }

    void GlfwInput::KeyCallback(GLFWwindow* window, int key, int  /*scancode*/, int action, int  /*mods*/) {
        ENGINE_ASSERT(window != nullptr, "GlfwInput::KeyCallback received a null window pointer");

        auto* input = static_cast<GlfwInput*>(glfwGetWindowUserPointer(window));
        ENGINE_ASSERT(input != nullptr, "GlfwInput::KeyCallback: window user pointer is null -- was Initialize called?");

        if (key >= 0 && key < 348) {
            if (action == GLFW_PRESS) {
                input->m_keys.set(key);
            } else if (action == GLFW_RELEASE) {
                input->m_keys.reset(key);
            }
        }
    }

} // namespace engine
