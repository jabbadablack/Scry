#ifndef ENGINE_OS_IWINDOW_HPP
#define ENGINE_OS_IWINDOW_HPP

namespace engine {

    struct NativeHandles {
        void* window;
        void* display;
    };

    class IWindow {
    public:
        virtual ~IWindow() = default;
        virtual void PollEvents() = 0;
        virtual void SwapBuffers() = 0;
        [[nodiscard]] virtual bool ShouldClose() const = 0;
        [[nodiscard]] virtual NativeHandles GetNativeHandles() const = 0;
    };

} // namespace engine

#endif // ENGINE_OS_IWINDOW_HPP
