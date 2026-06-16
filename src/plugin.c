#include <engine/plugin.h>
#include <engine/engine.h>
#include <engine/PluginAPI.h>
#include <engine/ecs.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define LIB_EXT ".dll"
#define LIB_LOAD(path) LoadLibraryA(path)
#define LIB_SYM(handle, name) GetProcAddress((HMODULE)handle, name)
#define LIB_FREE(handle) FreeLibrary((HMODULE)handle)
#else
#include <dlfcn.h>
#define LIB_EXT ".so"
#define LIB_LOAD(path) dlopen(path, RTLD_NOW)
#define LIB_SYM(handle, name) dlsym(handle, name)
#define LIB_FREE(handle) dlclose(handle)
#endif

typedef struct ScryPluginNode {
    void* handle;
    char name[256];
    struct ScryPluginNode* next;
} ScryPluginNode;

static ScryPluginNode* g_Plugins = NULL;

static void ScryPlugin_Log(const char* msg) {
    Scry_Log(msg);
}

static void* ScryPlugin_Alloc(size_t size) {
    return malloc(size);
}

static void ScryPlugin_Free(void* ptr) {
    free(ptr);
}

bool ScryPlugin_LoadSinglePlugin(ScryContext* ctx, const char* filepath) {
    void* handle = LIB_LOAD(filepath);
    if (!handle) {
        char buf[512];
        snprintf(buf, sizeof(buf), "[Plugin] Failed to load: %s", filepath);
        Scry_Log(buf);
        return false;
    }

    typedef void (*PluginInitFn)(const PluginAPI*);
    PluginInitFn init_fn = (PluginInitFn)LIB_SYM(handle, "PluginInit");
    if (!init_fn) {
        char buf[512];
        snprintf(buf, sizeof(buf), "[Plugin] No PluginInit in: %s", filepath);
        Scry_Log(buf);
        LIB_FREE(handle);
        return false;
    }

    PluginAPI api = {
        .ecs_world = ctx->ecs_world,
        .Log = ScryPlugin_Log,
        .Alloc = ScryPlugin_Alloc,
        .Free = ScryPlugin_Free,
        .SubmitTask = NULL // TODO: Link threading
    };

    init_fn(&api);

    ScryPluginNode* node = (ScryPluginNode*)malloc(sizeof(ScryPluginNode));
    node->handle = handle;
    strncpy(node->name, filepath, sizeof(node->name));
    node->next = g_Plugins;
    g_Plugins = node;

    char buf[512];
    snprintf(buf, sizeof(buf), "[Plugin] Loaded: %s", filepath);
    Scry_Log(buf);
    return true;
}

bool ScryPlugin_LoadPlugins(ScryContext* ctx) {
    (void)ctx;
    return true;
}

void ScryPlugin_UnloadPlugins(void) {
    ScryPluginNode* curr = g_Plugins;
    while (curr) {
        ScryPluginNode* next = curr->next;
        LIB_FREE(curr->handle);
        free(curr);
        curr = next;
    }
    g_Plugins = NULL;
}
