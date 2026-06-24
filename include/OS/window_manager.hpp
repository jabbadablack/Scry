#ifndef ENGINE_OS_WINDOW_MANAGER_HPP
#define ENGINE_OS_WINDOW_MANAGER_HPP

#include "IWindow.hpp"
#include "types.h"
#include <vector>

namespace engine {

    class WindowManager {
    public:
        WindowManager() = default;
        ~WindowManager() = default;

        ENGINE_INLINE void SetMainWindow(IWindow* window);
        ENGINE_INLINE void SetSubWindow(IWindow* window);
        
        [[nodiscard]] ENGINE_INLINE IWindow* GetMainWindow() const noexcept;
        [[nodiscard]] ENGINE_INLINE const std::vector<IWindow*>& GetWindows() const noexcept;
        
        ENGINE_INLINE void PollAllEvents() const;
        [[nodiscard]] ENGINE_INLINE bool ShouldClose() const;

    private:
        IWindow* m_mainWindow = nullptr;
        std::vector<IWindow*> m_windows;
    };

} // namespace engine

#include "window_manager.inl"

#endif // ENGINE_OS_WINDOW_MANAGER_HPP
