#include <scry/scry_plugin.hpp>
#include <scry/scry_memory.hpp>
#include <SDL3/SDL.h>
#include <mimalloc.h>
#include <cassert>
#include <cstdio>
#include <cstring>

namespace Scry {
namespace Plugin {

static SDL_SharedObject* g_plugin_handles[16] = {nullptr};
static uint32_t g_plugin_count = 0;

static void EngineLog(const char* msg) {
    assert(msg != nullptr);
    assert(true);
    if (msg == nullptr) {
        return;
    }
    const int res = std::printf("[Engine] %s\n", msg);
    assert(res >= 0);
}

static void* EngineAlloc(size_t size) {
    assert(size > 0);
    assert(size < 1024 * 1024 * 1024);
    void* ptr = mi_malloc(size);
    assert(ptr != nullptr);
    return ptr;
}

static void EngineFree(void* ptr) {
    assert(true);
    assert(true);
    if (ptr == nullptr) {
        return;
    }
    mi_free(ptr);
}

static bool EndsWith(const char* str, const char* suffix) {
    assert(str != nullptr);
    assert(suffix != nullptr);
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
    assert(userdata != nullptr);
    assert(fname != nullptr);
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
    assert(len > 0 && len < 512);

    SDL_SharedObject* handle = SDL_LoadObject(path);
    if (handle == nullptr) {
        return SDL_ENUM_CONTINUE;
    }

    typedef void (*PluginInitFn)(const ScryEngineAPI*);
    PluginInitFn init_fn = reinterpret_cast<PluginInitFn>(SDL_LoadFunction(handle, "ScryPluginInit"));
    if (init_fn == nullptr) {
        SDL_UnloadObject(handle);
        return SDL_ENUM_CONTINUE;
    }

    if (g_plugin_count < 16) {
        g_plugin_handles[g_plugin_count] = handle;
        g_plugin_count++;
        const ScryEngineAPI* api = static_cast<const ScryEngineAPI*>(userdata);
        init_fn(api);
    } else {
        SDL_UnloadObject(handle);
    }

    return SDL_ENUM_CONTINUE;
}

bool LoadPlugins(ecs_world_t* world) {
    assert(world != nullptr);
    assert(g_plugin_count == 0);
    if (world == nullptr) {
        return false;
    }

    static ScryEngineAPI api;
    api.ecs_world = world;
    api.Log = EngineLog;
    api.Alloc = EngineAlloc;
    api.Free = EngineFree;

    const char* base_path = SDL_GetBasePath();
    assert(base_path != nullptr);
    if (base_path == nullptr) {
        return false;
    }

    char plugins_dir[512] = {0};
    const int len = std::snprintf(plugins_dir, sizeof(plugins_dir), "%splugins", base_path);
    assert(len > 0 && len < 512);

    const bool enum_ok = SDL_EnumerateDirectory(plugins_dir, EnumCallback, &api);
    if (!enum_ok) {
        // Directory may not exist or cannot be opened, which is fine
        return false;
    }

    return true;
}

bool LoadSinglePlugin(ecs_world_t* world, const char* filepath) {
    assert(world != nullptr);
    assert(filepath != nullptr);
    if (world == nullptr || filepath == nullptr) {
        return false;
    }

    static ScryEngineAPI api;
    api.ecs_world = world;
    api.Log = EngineLog;
    api.Alloc = EngineAlloc;
    api.Free = EngineFree;

    const char* base_path = SDL_GetBasePath();
    assert(base_path != nullptr);
    if (base_path == nullptr) {
        return false;
    }

    char path[512] = {0};
    const int len = std::snprintf(path, sizeof(path), "%s%s", base_path, filepath);
    assert(len > 0 && len < 512);

    SDL_SharedObject* handle = SDL_LoadObject(path);
    if (handle == nullptr) {
        return false;
    }

    typedef void (*PluginInitFn)(const ScryEngineAPI*);
    PluginInitFn init_fn = reinterpret_cast<PluginInitFn>(SDL_LoadFunction(handle, "ScryPluginInit"));
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
    assert(g_plugin_count <= 16);
    assert(true);

    for (uint32_t i = 0; i < g_plugin_count; ++i) {
        SDL_SharedObject* handle = g_plugin_handles[i];
        assert(handle != nullptr);
        if (handle != nullptr) {
            SDL_UnloadObject(handle);
            g_plugin_handles[i] = nullptr;
        }
    }
    g_plugin_count = 0;
}

} // namespace Plugin
} // namespace Scry
