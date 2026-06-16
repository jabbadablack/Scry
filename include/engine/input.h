#pragma once
#include <engine/engine.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ScryKey {
    SCRY_KEY_NONE = 0,
    SCRY_KEY_A = 65, SCRY_KEY_B = 66, SCRY_KEY_C = 67, SCRY_KEY_D = 68, SCRY_KEY_E = 69, SCRY_KEY_F = 70, SCRY_KEY_G = 71, SCRY_KEY_H = 72, SCRY_KEY_I = 73, SCRY_KEY_J = 74,
    SCRY_KEY_K = 75, SCRY_KEY_L = 76, SCRY_KEY_M = 77, SCRY_KEY_N = 78, SCRY_KEY_O = 79, SCRY_KEY_P = 80, SCRY_KEY_Q = 81, SCRY_KEY_R = 82, SCRY_KEY_S = 83,
    SCRY_KEY_T = 84, SCRY_KEY_U = 85, SCRY_KEY_V = 86, SCRY_KEY_W = 87, SCRY_KEY_X = 88, SCRY_KEY_Y = 89, SCRY_KEY_Z = 90,
    SCRY_KEY_NUM0 = 48, SCRY_KEY_NUM1 = 49, SCRY_KEY_NUM2 = 50, SCRY_KEY_NUM3 = 51, SCRY_KEY_NUM4 = 52,
    SCRY_KEY_NUM5 = 53, SCRY_KEY_NUM6 = 54, SCRY_KEY_NUM7 = 55, SCRY_KEY_NUM8 = 56, SCRY_KEY_NUM9 = 57,
    SCRY_KEY_RETURN = 257, SCRY_KEY_ESCAPE = 256, SCRY_KEY_BACKSPACE = 259, SCRY_KEY_TAB = 258, SCRY_KEY_SPACE = 32,
    SCRY_KEY_F1 = 290, SCRY_KEY_F2 = 291, SCRY_KEY_F3 = 292, SCRY_KEY_F4 = 293, SCRY_KEY_F5 = 294, SCRY_KEY_F6 = 295,
    SCRY_KEY_F7 = 296, SCRY_KEY_F8 = 297, SCRY_KEY_F9 = 298, SCRY_KEY_F10 = 299, SCRY_KEY_F11 = 300, SCRY_KEY_F12 = 301,
    SCRY_KEY_RIGHT = 262, SCRY_KEY_LEFT = 263, SCRY_KEY_DOWN = 264, SCRY_KEY_UP = 265,
    SCRY_KEY_LCTRL = 341, SCRY_KEY_LSHIFT = 340, SCRY_KEY_LALT = 342,
    SCRY_KEY_RCTRL = 345, SCRY_KEY_RSHIFT = 344, SCRY_KEY_RALT = 346,
    SCRY_KEY_MOUSEL = 0,
    SCRY_KEY_MOUSER = 1,
    SCRY_KEY_MOUSEM = 2
} ScryKey;

typedef struct ScryInputState {
    uint8_t keys[64];
    int16_t mouse_x;
    int16_t mouse_y;
    float   mouse_dx;
    float   mouse_dy;
} ScryInputState;

typedef struct ScryInputBuffer {
    ScryInputState states[2];
    uint8_t write_index;
    uint8_t read_index;
} ScryInputBuffer;

ENGINE_API extern ScryInputBuffer g_ScryInput;

ENGINE_API void ScryInput_Swap(void);
ENGINE_API bool ScryInput_IsKeyDown(ScryKey key);
ENGINE_API bool ScryInput_IsRawKeyDown(uint32_t sc);
ENGINE_API void ScryInput_GetMousePos(int16_t* out_x, int16_t* out_y);

#ifdef __cplusplus
}
#endif
