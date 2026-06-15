#pragma once
#include <engine/engine.h>
#include <cstdint>

namespace Engine {
namespace Input {

enum class Key : uint32_t {
    None = 0,

    // Letters (matching GLFW keys)
    A = 65, B = 66, C = 67, D = 68, E = 69, F = 70, G = 71, H = 72, I = 73, J = 74,
    K = 75, L = 76, M = 77, N = 78, O = 79, P = 80, Q = 81, R = 82, S = 83,
    T = 84, U = 85, V = 86, W = 87, X = 88, Y = 89, Z = 90,

    // Numbers
    Num0 = 48, Num1 = 49, Num2 = 50, Num3 = 51, Num4 = 52,
    Num5 = 53, Num6 = 54, Num7 = 55, Num8 = 56, Num9 = 57,

    // Controls
    Return = 257, Escape = 256, Backspace = 259, Tab = 258, Space = 32,

    // Function keys
    F1 = 290, F2 = 291, F3 = 292, F4 = 293, F5 = 294, F6 = 295,
    F7 = 296, F8 = 297, F9 = 298, F10 = 299, F11 = 300, F12 = 301,

    // Arrows
    Right = 262, Left = 263, Down = 264, Up = 265,

    // Modifiers
    LCtrl = 341, LShift = 340, LAlt = 342,
    RCtrl = 345, RShift = 344, RAlt = 346,

    // Virtual Mouse Codes (GLFW mouse buttons are 0-7, mapping to custom range)
    MouseL = 0,
    MouseR = 1,
    MouseM = 2
};

struct ENGINE_API InputState {
    uint8_t keys[64] = {};  // 64 bytes - 512 bits; uint8_t enables SIMD block-reads without endianness concerns
    int16_t mouse_x  = 0;
    int16_t mouse_y  = 0;
    float   mouse_dx = 0.0f;
    float   mouse_dy = 0.0f;
};

struct ENGINE_API InputBuffer {
    InputState states[2];      // 136 bytes (Double buffer for Read/Write states)
    uint8_t write_index = 0;   // 1 byte
    uint8_t read_index = 1;    // 1 byte
    // Total size: 138 bytes (alignment 2, no tail padding).

    inline void Swap() {
        uint8_t temp = read_index;
        read_index = write_index;
        write_index = temp;
        // Carry over the active inputs to the next frame to persist held buttons
        states[write_index] = states[read_index];
        // Reset relative motion
        states[write_index].mouse_dx = 0.0f;
        states[write_index].mouse_dy = 0.0f;
    }

    inline bool IsKeyDown(Key key) const {
        uint32_t scancode = static_cast<uint32_t>(key);
        if (scancode < 512) {
            uint32_t idx = scancode / 8;
            uint32_t bit = scancode % 8;
            return (states[read_index].keys[idx] & (1u << bit)) != 0;
        }
        return false;
    }

    inline bool IsRawKeyDown(uint32_t scancode) const {
        if (scancode < 512) {
            uint32_t idx = scancode / 8;
            uint32_t bit = scancode % 8;
            return (states[read_index].keys[idx] & (1u << bit)) != 0;
        }
        return false;
    }

    inline void GetMousePos(int16_t& out_x, int16_t& out_y) const {
        out_x = states[read_index].mouse_x;
        out_y = states[read_index].mouse_y;
    }
};

// Global, pre-allocated double-buffer for input state
ENGINE_API extern InputBuffer g_input_buffer;

} // namespace Input
} // namespace Engine
