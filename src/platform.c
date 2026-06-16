#include <engine/platform.h>
#include <engine/input.h>
#include <engine/ecs.h>
#include <engine/renderer/core.h>
#include <engine/renderer/renderer.h>
#include <engine/plugin.h>
#include <engine/pipeline.h>
#include <engine/transform.h>
#include <engine/spatial.h>
#include <engine/camera.h>
#include <flecs.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#include <unistd.h>
#endif

ScryInputBuffer g_ScryInput = {0};

void Scry_Sleep(uint32_t ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)window; (void)scancode; (void)mods;
    if (key < 0 || key >= 512) return;
    const uint8_t w = g_ScryInput.write_index;
    ScryInputState* state = &g_ScryInput.states[w];
    const uint32_t idx = (uint32_t)key / 8;
    const uint32_t bit = (uint32_t)key % 8;
    if (action == GLFW_PRESS)   state->keys[idx] |=  (uint8_t)(1u << bit);
    else if (action == GLFW_RELEASE) state->keys[idx] &= (uint8_t)(~(1u << bit));
}

static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    (void)mods;
    if (button < 0 || button > 2) return;
    const uint8_t w = g_ScryInput.write_index;
    ScryInputState* state = &g_ScryInput.states[w];

    uint32_t sc = 0;
    if (button == GLFW_MOUSE_BUTTON_LEFT)   sc = (uint32_t)SCRY_KEY_MOUSEL;
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)  sc = (uint32_t)SCRY_KEY_MOUSER;
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE) sc = (uint32_t)SCRY_KEY_MOUSEM;

    const uint32_t idx = sc / 8, bit = sc % 8;
    if (action == GLFW_PRESS) {
        state->keys[idx] |= (uint8_t)(1u << bit);
        if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            if (glfwRawMouseMotionSupported())
                glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }
    } else if (action == GLFW_RELEASE) {
        state->keys[idx] &= (uint8_t)(~(1u << bit));
        if (button == GLFW_MOUSE_BUTTON_RIGHT)
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    const uint8_t w = g_ScryInput.write_index;
    ScryInputState* state = &g_ScryInput.states[w];
    static double last_x = 0, last_y = 0;
    static bool first = true;
    if (first) { last_x = xpos; last_y = ypos; first = false; }
    state->mouse_dx += (float)(xpos - last_x);
    state->mouse_dy += (float)(ypos - last_y);
    last_x = xpos; last_y = ypos;
    state->mouse_x = (int16_t)xpos;
    state->mouse_y = (int16_t)ypos;
}

static void ErrorCallback(int error, const char* description) {
    fprintf(stderr, "[GLFW Error] %d: %s\n", error, description);
}

void* ScryPlatform_InitWindow(const char* title, int32_t width, int32_t height) {
    glfwSetErrorCallback(ErrorCallback);
    if (!glfwInit()) return NULL;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(width, height, title ? title : "Scry", NULL, NULL);
    if (window) {
        glfwSetKeyCallback(window, KeyCallback);
        glfwSetMouseButtonCallback(window, MouseButtonCallback);
        glfwSetCursorPosCallback(window, CursorPosCallback);
    }
    return window;
}

void ScryPlatform_PumpEvents(ScryContext* ctx) {
    const uint8_t w = g_ScryInput.write_index;
    const uint8_t r = g_ScryInput.read_index;
    g_ScryInput.states[w] = g_ScryInput.states[r];
    g_ScryInput.states[w].mouse_dx = 0.0f;
    g_ScryInput.states[w].mouse_dy = 0.0f;
    glfwPollEvents();
    if (glfwWindowShouldClose((GLFWwindow*)ctx->window_handle))
        ctx->running = 0;
    ScryInput_Swap();
}

uint64_t ScryPlatform_GetTime(void) {
    return (uint64_t)(glfwGetTime() * 1000.0);
}

void ScryPlatform_DestroyWindow(void* handle) {
    if (handle) glfwDestroyWindow((GLFWwindow*)handle);
    glfwTerminate();
}

void ScryInput_Swap(void) {
    uint8_t temp = g_ScryInput.read_index;
    g_ScryInput.read_index = g_ScryInput.write_index;
    g_ScryInput.write_index = temp;
}

bool ScryInput_IsKeyDown(ScryKey key) {
    uint32_t sc = (uint32_t)key;
    if (sc >= 512) return false;
    return (g_ScryInput.states[g_ScryInput.read_index].keys[sc / 8] & (1u << (sc % 8))) != 0;
}

bool ScryInput_IsRawKeyDown(uint32_t sc) {
    if (sc >= 512) return false;
    return (g_ScryInput.states[g_ScryInput.read_index].keys[sc / 8] & (1u << (sc % 8))) != 0;
}

void ScryInput_GetMousePos(int16_t* out_x, int16_t* out_y) {
    *out_x = g_ScryInput.states[g_ScryInput.read_index].mouse_x;
    *out_y = g_ScryInput.states[g_ScryInput.read_index].mouse_y;
}

static void (*g_app_log)(const char*) = NULL;

ENGINE_API ScryError Scry_Run(const ScryAppConfig* config) {
    if (!config || !config->OnInit || !config->OnShutdown) return SCRY_ERR_PLATFORM_INIT;
    g_app_log = config->OnLog;

    ScryContext ctx = {0};
    void* window = ScryPlatform_InitWindow(config->title, config->window_width, config->window_height);
    if (!window) return SCRY_ERR_PLATFORM_INIT;

    if (!ScryGraphics_Init(window)) return SCRY_ERR_GRAPHICS_INIT;

    ScryECS_InitOSAPI();
    struct ecs_world_t* world = ScryECS_CreateWorld();
    if (!world) return SCRY_ERR_ECS_INIT;

    ScryPipeline_Init(world);
    ScryTransform_Init(world);
    ScrySpatial_Init(world);
    ScryCamera_Init(world);
    ScryRenderer_Init(world);

    ctx.ecs_world = world;
    ctx.window_handle = window;
    ctx.user_data = config->user_data;
    ctx.start_time = ScryPlatform_GetTime();
    ctx.window_width = config->window_width;
    ctx.window_height = config->window_height;
    ctx.initialized = 1;
    ctx.running = 1;

    config->OnInit(&ctx);
    Scry_Log("Starting main loop...");

    uint64_t last_tick = ScryPlatform_GetTime();
    while (ctx.running) {
        uint64_t frame_start = ScryPlatform_GetTime();

        ScryPlatform_PumpEvents(&ctx);
        if (!ctx.running) break;
        if (ScryInput_IsKeyDown(SCRY_KEY_ESCAPE)) { ctx.running = 0; break; }

        uint64_t now = ScryPlatform_GetTime();
        float dt = (float)(now - last_tick) / 1000.0f;
        last_tick = now;

        ScryGraphics_BeginFrame();
        // Scry_Log("Advancing world...");
        if (!ecs_progress(world, dt)) { Scry_Log("ecs_progress returned false"); ctx.running = 0; break; }
        ScryGraphics_Present();

        uint64_t frame_end = ScryPlatform_GetTime();
        uint64_t elapsed = frame_end - frame_start;
        if (elapsed < 16) {
            Scry_Sleep(16 - (uint32_t)elapsed);
        }
    }

    config->OnShutdown(&ctx);

    ScryRenderer_Shutdown();
    ecs_fini(world);
    ScryECS_ShutdownOSAPI();
    ScryGraphics_Shutdown();
    ScryPlatform_DestroyWindow(window);

    return SCRY_SUCCESS;
}

ENGINE_API void Scry_RequestExit(ScryContext* ctx) {
    if (ctx) ctx->running = 0;
}

ENGINE_API void* Scry_GetUserData(const ScryContext* ctx) {
    return ctx ? ctx->user_data : NULL;
}

ENGINE_API struct ecs_world_t* Scry_GetWorld(const ScryContext* ctx) {
    return ctx ? ctx->ecs_world : NULL;
}

ENGINE_API void Scry_Log(const char* msg) {
    if (!msg) return;
    if (g_app_log) g_app_log(msg);
    else { printf("[Scry] %s\n", msg); fflush(stdout); }
}
