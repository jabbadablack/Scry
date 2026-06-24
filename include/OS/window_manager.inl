#ifndef ENGINE_OS_WINDOW_MANAGER_INL
#define ENGINE_OS_WINDOW_MANAGER_INL

#include "../debug/assert.h"
#include "../debug/logger.hpp"

namespace engine {

    ENGINE_INLINE void WindowManager::RegisterMainWindow(IWindow* window) {
        ENGINE_ASSERT(window != nullptr, "Cannot register a null main window");
        ENGINE_ASSERT(m_mainWindow == nullptr, "A main window is already registered");

        m_mainWindow = window;
        m_windows.push_back(window);
        ENGINE_LOG_INFO("WindowManager: main window registered");
    }

    ENGINE_INLINE void WindowManager::RegisterSubWindow(IWindow* window) {
        ENGINE_ASSERT(window != nullptr, "Cannot register a null sub-window");
        ENGINE_ASSERT(m_mainWindow != nullptr, "Main window must be registered before sub-windows");

        m_windows.push_back(window);
        ENGINE_LOG_INFO("WindowManager: sub-window registered");
    }

    ENGINE_INLINE IWindow* WindowManager::GetMainWindow() const noexcept {
        ENGINE_ASSERT(m_mainWindow != nullptr, "GetMainWindow called before any window was registered");
        return m_mainWindow;
    }

    ENGINE_INLINE const std::vector<IWindow*>& WindowManager::GetWindows() const noexcept {
        ENGINE_ASSERT(!m_windows.empty(), "GetWindows called but no windows are registered");
        return m_windows;
    }

    ENGINE_INLINE void WindowManager::PollAllEvents() const {
        ENGINE_ASSERT(m_mainWindow != nullptr, "Cannot poll events: no main window registered");
        ENGINE_ASSERT(!m_windows.empty(), "Cannot poll events: window list is empty");

        for (auto* window : m_windows) {
            window->PollEvents();
        }
    }

    ENGINE_INLINE bool WindowManager::ShouldClose() const {
        ENGINE_ASSERT(m_mainWindow != nullptr, "ShouldClose called with no main window registered");
        ENGINE_ASSERT(!m_windows.empty(), "ShouldClose called with empty window list");

        return m_mainWindow->ShouldClose();
    }

} // namespace engine

#endif // ENGINE_OS_WINDOW_MANAGER_INL
