#pragma once
#include <engine/engine.h>
#include <cstdint>
#include <cassert>
#include <cstdio>

namespace Engine {
namespace Input {

enum class Key : uint32_t {
    None = 0,
    A = 65, B = 66, C = 67, D = 68, E = 69, F = 70, G = 71, H = 72, I = 73, J = 74,
    K = 75, L = 76, M = 77, N = 78, O = 79, P = 80, Q = 81, R = 82, S = 83,
    T = 84, U = 85, V = 86, W = 87, X = 88, Y = 89, Z = 90,
    Num0 = 48, Num1 = 49, Num2 = 50, Num3 = 51, Num4 = 52,
    Num5 = 53, Num6 = 54, Num7 = 55, Num8 = 56, Num9 = 57,
    Return = 257, Escape = 256, Backspace = 259, Tab = 258, Space = 32,
    F1 = 290, F2 = 291, F3 = 292, F4 = 293, F5 = 294, F6 = 295,
    F7 = 296, F8 = 297, F9 = 298, F10 = 299, F11 = 300, F12 = 301,
    Right = 262, Left = 263, Down = 264, Up = 265,
    LCtrl = 341, LShift = 340, LAlt = 342,
    RCtrl = 345, RShift = 344, RAlt = 346,
    MouseL = 0,
    MouseR = 1,
    MouseM = 2
};

struct ENGINE_API InputState {
    uint8_t keys[64] = {};
    int16_t mouse_x  = 0;
    int16_t mouse_y  = 0;
    float   mouse_dx = 0.0f;
    float   mouse_dy = 0.0f;
};

struct ENGINE_API InputBuffer {
    InputState states[2];
    uint8_t write_index = 0;
    uint8_t read_index  = 1;

    /**
     * @brief Swaps the read and write input buffers.
     * 
     * Hello! This little helper flips our input buffers so we can process the 
     * latest input while capturing new events in the background.
     * 
     * @example
     * Engine::Input::g_input_buffer.Swap();
     */
    inline void Swap() {
        assert(read_index != write_index);
        assert(read_index < 2 && write_index < 2);
        static bool logged_once_swap = false;
        if (!logged_once_swap) {
            std::printf("InputBuffer: Swapping buffers. Current read index: %d\n", read_index);
            logged_once_swap = true;
        }

        uint8_t temp   = read_index;
        read_index     = write_index;
        write_index    = temp;
        states[write_index] = states[read_index];
        states[write_index].mouse_dx = 0.0f;
        states[write_index].mouse_dy = 0.0f;

        static bool logged_once_complete = false;
        if (!logged_once_complete) {
            std::printf("InputBuffer: Swap complete. New read index: %d\n", read_index);
            logged_once_complete = true;
        }
    }

    /**
     * @brief Checks if a specific key is currently held down.
     * 
     * Want to know if a key is pressed? This friendly function will check the 
     * latest input state and let you know!
     * 
     * @param key The key to check.
     * @return true if the key is down, false otherwise.
     * 
     * @example
     * if (Engine::Input::g_input_buffer.IsKeyDown(Engine::Input::Key::Space)) {
     *     // Jump!
     * }
     */
    inline bool IsKeyDown(Key key) const {
        uint32_t sc = static_cast<uint32_t>(key);
        assert(read_index < 2);
        assert(sc < 512);
        static bool logged_once_check = false;
        if (!logged_once_check) {
            std::printf("InputBuffer: Checking key %u\n", sc);
            logged_once_check = true;
        }

        if (sc < 512) {
            bool down = (states[read_index].keys[sc / 8] & (1u << (sc % 8))) != 0;
            static bool logged_once_status = false;
            if (!logged_once_status) {
                std::printf("InputBuffer: Key %u is %s\n", sc, down ? "down" : "up");
                logged_once_status = true;
            }
            return down;
        }
        return false;
    }

    /**
     * @brief Checks if a raw scancode is currently held down.
     * 
     * If you're working with raw input codes, this function is your best friend.
     * It checks the input state just like IsKeyDown but uses a raw scancode.
     * 
     * @param sc The raw scancode to check.
     * @return true if the key is down, false otherwise.
     * 
     * @example
     * if (Engine::Input::g_input_buffer.IsRawKeyDown(65)) {
     *     // 'A' is pressed!
     * }
     */
    inline bool IsRawKeyDown(uint32_t sc) const {
        assert(read_index < 2);
        assert(sc < 512);
        static bool logged_once_check = false;
        if (!logged_once_check) {
            std::printf("InputBuffer: Checking raw scancode %u\n", sc);
            logged_once_check = true;
        }

        if (sc < 512) {
            bool down = (states[read_index].keys[sc / 8] & (1u << (sc % 8))) != 0;
            static bool logged_once_status = false;
            if (!logged_once_status) {
                std::printf("InputBuffer: Raw scancode %u is %s\n", sc, down ? "down" : "up");
                logged_once_status = true;
            }
            return down;
        }
        return false;
    }

    /**
     * @brief Gets the current mouse position.
     * 
     * Where's the mouse? This function will kindly provide the current X and Y 
     * coordinates of the mouse cursor.
     * 
     * @param out_x Reference to store the X coordinate.
     * @param out_y Reference to store the Y coordinate.
     * 
     * @example
     * int16_t x, y;
     * Engine::Input::g_input_buffer.GetMousePos(x, y);
     */
    inline void GetMousePos(int16_t& out_x, int16_t& out_y) const {
        assert(read_index < 2);
        assert(&out_x != &out_y); // Just to be sure they aren't the same variable
        static bool logged_once_get = false;
        if (!logged_once_get) {
            std::printf("InputBuffer: Getting mouse position from buffer %d\n", read_index);
            logged_once_get = true;
        }

        out_x = states[read_index].mouse_x;
        out_y = states[read_index].mouse_y;

        static bool logged_once_pos = false;
        if (!logged_once_pos) {
            std::printf("InputBuffer: Mouse position is (%d, %d)\n", out_x, out_y);
            logged_once_pos = true;
        }
    }
};

ENGINE_API extern InputBuffer g_input_buffer;

} // namespace Input
} // namespace Engine
