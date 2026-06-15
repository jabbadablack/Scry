#include <engine/plugin.hpp>
#include <engine/engine_context.hpp>
#include <engine/platform.hpp>
#include <engine/memory.hpp>
#include <yyjson.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

namespace Engine {
namespace Plugin {

struct PluginDescriptor { std::string name; std::string library_path; };

#ifdef _WIN32
typedef HMODULE LibraryHandle;
#else
typedef void* LibraryHandle;
#endif

static LibraryHandle g_plugin_handles[16] = {nullptr};
static uint32_t      g_plugin_count       = 0;
static std::vector<PluginDescriptor> g_registry;

void SetQuillActive(bool active) { (void)active; }

static void EngineLogInternal(const char* msg) { EngineLog(msg); }
static void* EngineAlloc(size_t size) { return std::malloc(size); }
static void  EngineFree(void* ptr)   { std::free(ptr); }

bool LoadPlugins(Context* ctx) {
    (void)ctx;
    g_registry.clear();

    fs::path base = fs::current_path();
    fs::path root = base / "plugins";
    if (!fs::exists(root)) root = base / ".." / ".." / "plugins";
    if (!fs::exists(root) || !fs::is_directory(root)) return false;

    for (const auto& entry : fs::directory_iterator(root)) {
        if (!entry.is_directory()) continue;
        fs::path json_path = entry.path() / "plugin.json";
        if (!fs::exists(json_path)) continue;

        std::FILE* f = std::fopen(json_path.string().c_str(), "rb");
        if (!f) continue;
        std::fseek(f, 0, SEEK_END);
        long fsize = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        if (fsize <= 0) { std::fclose(f); continue; }

        char* buf = static_cast<char*>(std::malloc(static_cast<size_t>(fsize)));
        if (!buf) { std::fclose(f); continue; }
        size_t nread = std::fread(buf, 1, static_cast<size_t>(fsize), f);
        std::fclose(f);

        yyjson_doc* doc = yyjson_read(buf, nread, 0);
        if (doc) {
            yyjson_val* root_val  = yyjson_doc_get_root(doc);
            yyjson_val* name_val  = yyjson_obj_get(root_val, "name");
            if (name_val && yyjson_is_str(name_val)) {
                PluginDescriptor desc;
                desc.name = yyjson_get_str(name_val);
#ifdef _WIN32
                const char* suffix = ".dll";
#else
                const char* suffix = ".so";
#endif
                for (const auto& lib_entry : fs::directory_iterator(entry.path())) {
                    if (lib_entry.path().extension().string() == suffix) {
                        desc.library_path = lib_entry.path().string();
                        g_registry.push_back(desc);
                        break;
                    }
                }
            }
            yyjson_doc_free(doc);
        }
        std::free(buf);
    }
    return true;
}

bool LoadSinglePlugin(Context* ctx, const char* name) {
    if (!ctx || !name) return false;
    const PluginDescriptor* found = nullptr;
    for (const auto& d : g_registry) { if (d.name == name) { found = &d; break; } }
    if (!found) return false;

    static PluginAPI api;
    api.ecs_world  = ctx->ecs_world;
    api.Log        = EngineLogInternal;
    api.Alloc      = EngineAlloc;
    api.Free       = EngineFree;
    api.SubmitTask = nullptr;

#ifdef _WIN32
    HMODULE handle = LoadLibraryA(found->library_path.c_str());
    if (!handle) return false;
    typedef void (*PluginInitFn)(const PluginAPI*);
    PluginInitFn init_fn = reinterpret_cast<PluginInitFn>(
        reinterpret_cast<void*>(GetProcAddress(handle, "PluginInit")));
    if (!init_fn) { FreeLibrary(handle); return false; }
#else
    void* handle = dlopen(found->library_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) { std::fprintf(stderr, "[Plugin] dlopen: %s\n", dlerror()); return false; }
    typedef void (*PluginInitFn)(const PluginAPI*);
    PluginInitFn init_fn = reinterpret_cast<PluginInitFn>(dlsym(handle, "PluginInit"));
    if (!init_fn) { dlclose(handle); return false; }
#endif

    if (g_plugin_count < 16) {
        g_plugin_handles[g_plugin_count++] = handle;
        init_fn(&api);
        return true;
    }
#ifdef _WIN32
    FreeLibrary(handle);
#else
    dlclose(handle);
#endif
    return false;
}

void UnloadPlugins() {
    for (uint32_t i = 0; i < g_plugin_count; ++i) {
        if (!g_plugin_handles[i]) continue;
#ifdef _WIN32
        FreeLibrary(g_plugin_handles[i]);
#else
        dlclose(g_plugin_handles[i]);
#endif
        g_plugin_handles[i] = nullptr;
    }
    g_plugin_count = 0;
    g_registry.clear();
}

} // namespace Plugin
} // namespace Engine
