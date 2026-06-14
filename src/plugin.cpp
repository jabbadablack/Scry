#include <engine/plugin.hpp>
#include <engine/engine_context.hpp>
#include <engine/platform.hpp>
#include <engine/memory.hpp>
#include <engine/job_system.hpp>
#include <SDL3/SDL.h>
#include <mimalloc.h>
#include <libassert/assert.hpp>
#include <yyjson.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>

namespace Engine {
namespace Plugin {

struct PluginDescriptor {
    std::string name;
    std::string library_path;
};

static SDL_SharedObject* g_plugin_handles[16] = {nullptr};
static uint32_t g_plugin_count = 0;
static std::vector<PluginDescriptor> g_registry;

void SetQuillActive(bool active) {
    (void)active;
}

static void EngineLogInternal(const char* msg) {
    EngineLog(msg);
}

static void* EngineAlloc(size_t size) {
    return mi_malloc(size);
}

static void EngineFree(void* ptr) {
    if (ptr) mi_free(ptr);
}

struct LibSearchData {
    const char* suffix;
    char result[512];
};

static SDL_EnumerationResult SDLCALL LibSearchCallback(void* userdata, const char* dirname, const char* fname) {
    LibSearchData* data = static_cast<LibSearchData*>(userdata);
    const size_t fname_len = std::strlen(fname);
    const size_t suffix_len = std::strlen(data->suffix);
    
    if (fname_len >= suffix_len && std::strcmp(fname + (fname_len - suffix_len), data->suffix) == 0) {
        std::snprintf(data->result, 512, "%s/%s", dirname, fname);
        return SDL_ENUM_FAILURE; 
    }
    return SDL_ENUM_CONTINUE;
}

static SDL_EnumerationResult SDLCALL RegistryEnumCallback(void* userdata, const char* dirname, const char* fname) {
    (void)userdata;
    if (std::strstr(fname, ".dll") || std::strstr(fname, ".so")) return SDL_ENUM_CONTINUE;

    char plugin_dir[512];
    std::snprintf(plugin_dir, sizeof(plugin_dir), "%s/%s", dirname, fname);
    
    char json_path[512];
    std::snprintf(json_path, sizeof(json_path), "%s/plugin.json", plugin_dir);
    
    size_t size = 0;
    void* buffer = SDL_LoadFile(json_path, &size);
    if (!buffer) return SDL_ENUM_CONTINUE;
    
    yyjson_doc* doc = yyjson_read(static_cast<const char*>(buffer), size, 0);
    if (doc) {
        yyjson_val* root = yyjson_doc_get_root(doc);
        yyjson_val* name_val = yyjson_obj_get(root, "name");
        if (name_val && yyjson_is_str(name_val)) {
            const char* name = yyjson_get_str(name_val);
            
            PluginDescriptor desc;
            desc.name = name;
            
            LibSearchData search;
            search.result[0] = '\0';
#ifdef _WIN32
            search.suffix = ".dll";
#else
            search.suffix = ".so";
#endif
            SDL_EnumerateDirectory(plugin_dir, LibSearchCallback, &search);
            if (search.result[0] != '\0') {
                desc.library_path = search.result;
                g_registry.push_back(desc);
            }
        }
        yyjson_doc_free(doc);
    }
    SDL_free(buffer);
    
    return SDL_ENUM_CONTINUE;
}

bool LoadPlugins(Context* ctx) {
    (void)ctx;
    const char* base_path = SDL_GetBasePath();
    if (!base_path) return false;

    char plugins_root[512];
    std::snprintf(plugins_root, sizeof(plugins_root), "%splugins", base_path);

    g_registry.clear();
    SDL_EnumerateDirectory(plugins_root, RegistryEnumCallback, nullptr);

    return true;
}

bool LoadSinglePlugin(Context* ctx, const char* name) {
    if (ctx == nullptr || name == nullptr) return false;

    const PluginDescriptor* found = nullptr;
    for (const auto& desc : g_registry) {
        if (desc.name == name) {
            found = &desc;
            break;
        }
    }

    if (!found) return false;

    static PluginAPI api;
    api.ecs_world  = ctx->ecs_world;
    api.Log        = EngineLogInternal;
    api.Alloc      = EngineAlloc;
    api.Free       = EngineFree;
    api.SubmitTask = Engine::Jobs::SubmitTask;

    SDL_SharedObject* handle = SDL_LoadObject(found->library_path.c_str());
    if (handle == nullptr) return false;

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
    for (uint32_t i = 0; i < g_plugin_count; ++i) {
        if (g_plugin_handles[i]) {
            SDL_UnloadObject(g_plugin_handles[i]);
            g_plugin_handles[i] = nullptr;
        }
    }
    g_plugin_count = 0;
    g_registry.clear();
}

} // namespace Plugin
} // namespace Engine
