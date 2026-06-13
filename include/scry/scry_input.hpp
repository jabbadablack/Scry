#pragma once
#include <scry/core.hpp>
#include <cstdint>

namespace Scry {
namespace Input {

enum class Key : uint32_t {
    None = 0,

    // Letters (matching SDL scancodes)
    A = 4, B = 5, C = 6, D = 7, E = 8, F = 9, G = 10, H = 11, I = 12, J = 13,
    K = 14, L = 15, M = 16, N = 17, O = 18, P = 19, Q = 20, R = 21, S = 22,
    T = 23, U = 24, V = 25, W = 26, X = 27, Y = 28, Z = 29,

    // Numbers
    Num1 = 30, Num2 = 31, Num3 = 32, Num4 = 33, Num5 = 34,
    Num6 = 35, Num7 = 36, Num8 = 37, Num9 = 38, Num0 = 39,

    // Controls
    Return = 40, Escape = 41, Backspace = 42, Tab = 43, Space = 44,

    // Function keys
    F1 = 58, F2 = 59, F3 = 60, F4 = 61, F5 = 62, F6 = 63,
    F7 = 64, F8 = 65, F9 = 66, F10 = 67, F11 = 68, F12 = 69,

    // Arrows
    Right = 79, Left = 80, Down = 81, Up = 82,

    // Modifiers
    LCtrl = 224, LShift = 225, LAlt = 226,
    RCtrl = 228, RShift = 229, RAlt = 230,

    // Virtual Mouse Codes (mapped to unused space above standard scancodes)
    MouseL = 510,
    MouseR = 511
};

struct SCRY_API InputState {
    uint64_t keys[8] = {0};    // 64 bytes - 512 bits for tracking every possible SDL scancode + mouse buttons
    int16_t mouse_x = 0;       // 2 bytes
    int16_t mouse_y = 0;       // 2 bytes
    // Total size: 68 bytes (padded to 72). Zero padding holes.
};

struct SCRY_API InputBuffer {
    InputState states[2];      // 136 bytes (Double buffer for Read/Write states)
    uint8_t write_index = 0;   // 1 byte
    uint8_t read_index = 1;    // 1 byte
    // Total size: 138 bytes (padded to 144). Sorted largest to smallest members.

    inline void Swap() {
        uint8_t temp = read_index;
        read_index = write_index;
        write_index = temp;
        // Carry over the active inputs to the next frame to persist held buttons
        states[write_index] = states[read_index];
    }

    inline bool IsKeyDown(Key key) const {
        uint32_t scancode = static_cast<uint32_t>(key);
        if (scancode < 512) {
            uint32_t idx = scancode / 64;
            uint32_t bit = scancode % 64;
            return (states[read_index].keys[idx] & (1ULL << bit)) != 0;
        }
        return false;
    }

    inline bool IsRawKeyDown(uint32_t scancode) const {
        if (scancode < 512) {
            uint32_t idx = scancode / 64;
            uint32_t bit = scancode % 64;
            return (states[read_index].keys[idx] & (1ULL << bit)) != 0;
        }
        return false;
    }

    inline void GetMousePos(int16_t& out_x, int16_t& out_y) const {
        out_x = states[read_index].mouse_x;
        out_y = states[read_index].mouse_y;
    }
};

// Global, pre-allocated double-buffer for input state
SCRY_API extern InputBuffer g_input_buffer;

} // namespace Input
} // namespace Scry
