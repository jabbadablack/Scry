#ifndef ENGINE_OS_IINPUT_HPP
#define ENGINE_OS_IINPUT_HPP

namespace engine {

    class IInput {
    public:
        virtual ~IInput() = default;
        virtual void Update() = 0;
        virtual bool IsKeyPressed(int key) const = 0;
        virtual bool IsKeyHeld(int key) const = 0;
        virtual bool IsKeyReleased(int key) const = 0;
    };

} // namespace engine

#endif // ENGINE_OS_IINPUT_HPP
