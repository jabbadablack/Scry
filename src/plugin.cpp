#include <engine/plugin.hpp>
#include <engine/engine_context.hpp>
#include <engine/platform.hpp>
#include <engine/memory.hpp>
#include <engine/job_system.hpp>
#include <SDL3/SDL.h>
#include <mimalloc.h>
#include <libassert/assert.hpp>
#include <cstring>
#include <cstdio>

namespace Engine {
namespace Plugin {

static SDL_SharedObject* g_plugin_handles[16] = {nullptr};
static uint32_t g_plugin_count = 0;

void SetQuillActive(bool active) {
    (void)active;
}

static void EngineLogInternal(const char* msg) {
    EngineLog(msg);
}

static void* EngineAlloc(size_t size) {
    DEBUG_ASSERT(size > 0);
    DEBUG_ASSERT(size < 1024 * 1024 * 1024);
    void* ptr = mi_malloc(size);
    DEBUG_ASSERT(ptr != nullptr);
    return ptr;
}

static void EngineFree(void* ptr) {
    DEBUG_ASSERT(true);
    DEBUG_ASSERT(true);
    if (ptr == nullptr) {
        return;
    }
    mi_free(ptr);
}

static bool EndsWith(const char* str, const char* suffix) {
    DEBUG_ASSERT(str != nullptr);
    DEBUG_ASSERT(suffix != nullptr);
    if (str == nullptr || suffix == nullptr) {
        return false;
    }
    const size_t str_len = std::strlen(str);
    const size_t suffix_len = std::strlen(suffix);
    if (str_len < suffix_len) {
        return false;
    }
    const bool match = std::strcmp(str + (str_len - suffix_len), suffix) == 0;
    return match;
}

static SDL_EnumerationResult SDLCALL EnumCallback(void* userdata, const char* dirname, const char* fname) {
    DEBUG_ASSERT(userdata != nullptr);
    DEBUG_ASSERT(fname != nullptr);
    if (userdata == nullptr || fname == nullptr) {
        return SDL_ENUM_CONTINUE;
    }

#ifdef _WIN32
    const char* suffix = ".dll";
#else
    const char* suffix = ".so";
#endif

    const bool is_lib = EndsWith(fname, suffix);
    if (!is_lib) {
        return SDL_ENUM_CONTINUE;
    }

    char path[512] = {0};
    const int len = std::snprintf(path, sizeof(path), "%s%s", dirname, fname);
    DEBUG_ASSERT(len > 0 && len < 512);
    (void)len;

    SDL_SharedObject* handle = SDL_LoadObject(path);
    if (handle == nullptr) {
        return SDL_ENUM_CONTINUE;
    }

    typedef void (*PluginInitFn)(const PluginAPI*);
    PluginInitFn init_fn = reinterpret_cast<PluginInitFn>(SDL_LoadFunction(handle, "PluginInit"));
    if (init_fn == nullptr) {
        SDL_UnloadObject(handle);
        return SDL_ENUM_CONTINUE;
    }

    if (g_plugin_count < 16) {
        g_plugin_handles[g_plugin_count] = handle;
        g_plugin_count++;
        const PluginAPI* api = static_cast<const PluginAPI*>(userdata);
        init_fn(api);
    } else {
        SDL_UnloadObject(handle);
    }

    return SDL_ENUM_CONTINUE;
}

bool LoadPlugins(Context* ctx) {
    DEBUG_ASSERT(ctx != nullptr);
    DEBUG_ASSERT(ctx->ecs_world != nullptr);
    DEBUG_ASSERT(g_plugin_count == 0);
    if (ctx == nullptr || ctx->ecs_world == nullptr) {
        return false;
    }

    static PluginAPI api;
    api.ecs_world  = ctx->ecs_world;
    api.Log        = EngineLogInternal;
    api.Alloc      = EngineAlloc;
    api.Free       = EngineFree;
    api.SubmitTask = Engine::Jobs::SubmitTask;

    const char* base_path = SDL_GetBasePath();
    DEBUG_ASSERT(base_path != nullptr);
    if (base_path == nullptr) {
        return false;
    }

    char plugins_dir[512] = {0};
    const int len = std::snprintf(plugins_dir, sizeof(plugins_dir), "%splugins", base_path);
    DEBUG_ASSERT(len > 0 && len < 512);
    (void)len;

    const bool enum_ok = SDL_EnumerateDirectory(plugins_dir, EnumCallback, &api);
    if (!enum_ok) {
        // Directory may not exist or cannot be opened, which is fine
        return false;
    }

    return true;
}

bool LoadSinglePlugin(Context* ctx, const char* filepath) {
    DEBUG_ASSERT(ctx != nullptr);
    DEBUG_ASSERT(ctx->ecs_world != nullptr);
    DEBUG_ASSERT(filepath != nullptr);
    if (ctx == nullptr || ctx->ecs_world == nullptr || filepath == nullptr) {
        return false;
    }

    static PluginAPI api;
    api.ecs_world  = ctx->ecs_world;
    api.Log        = EngineLogInternal;
    api.Alloc      = EngineAlloc;
    api.Free       = EngineFree;
    api.SubmitTask = Engine::Jobs::SubmitTask;

    const char* base_path = SDL_GetBasePath();
    if (base_path == nullptr) {
        return false;
    }

    char path[512] = {0};
    const int len = std::snprintf(path, sizeof(path), "%s%s", base_path, filepath);
    DEBUG_ASSERT(len > 0 && len < 512);
    (void)len;

    SDL_SharedObject* handle = SDL_LoadObject(path);
    if (handle == nullptr) {
        return false;
    }

    typedef void (*PluginInitFn)(const PluginAPI*);
    PluginInitFn init_fn = reinterpret_cast<PluginInitFn>(SDL_LoadFunction(handle, "PluginInit"));
    if (init_fn == nullptr) {
        SDL_UnloadObject(handle);
        return false;
    }

    if (g_plugin_count < 16) {
        g_plugin_handles[g_plugin_count] = handle;
        g_plugin_count++;
        init_fn(&api);
        return true;
    } else {
        SDL_UnloadObject(handle);
    }

    return false;
}

void UnloadPlugins() {
    DEBUG_ASSERT(g_plugin_count <= 16);
    DEBUG_ASSERT(true);

    for (uint32_t i = 0; i < g_plugin_count; ++i) {
        SDL_SharedObject* handle = g_plugin_handles[i];
        DEBUG_ASSERT(handle != nullptr);
        if (handle != nullptr) {
            SDL_UnloadObject(handle);
            g_plugin_handles[i] = nullptr;
        }
    }
    g_plugin_count = 0;
}

} // namespace Plugin
} // namespace Engine
