#include <engine/json.h>
#include <engine/platform.h>
#include <engine/math.h>
#include <engine/memory.h>
#include <engine/plugin.h>
#include <engine/ecs.h>
#include <yyjson.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t HashString(const char* str) {
    uint64_t hash = 14695981039346656037ULL;
    while (*str) {
        hash ^= (uint64_t)((unsigned char)*str++);
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void* YyjsonMalloc(void* ctx, size_t size) {
    (void)ctx; return malloc(size);
}

static void* YyjsonRealloc(void* ctx, void* ptr, size_t old_size, size_t new_size) {
    (void)ctx; (void)old_size; return realloc(ptr, new_size);
}

static void  YyjsonFree(void* ctx, void* ptr) {
    (void)ctx; free(ptr);
}

static char* ReadFileToBuffer(const char* filepath, size_t* out_size) {
    FILE* f = fopen(filepath, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    const long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc((size_t)file_size + 1);
    if (!buf) { fclose(f); return NULL; }
    *out_size = fread(buf, 1, (size_t)file_size, f);
    buf[*out_size] = '\0';
    fclose(f);
    return buf;
}

bool ScryJSON_LoadProjectConfig(ScryContext* ctx, const char* filepath) {
    if (!ctx || !ctx->ecs_world) return false;
    if (!filepath || !strlen(filepath)) filepath = "project.json";

    size_t size = 0;
    char* buf = ReadFileToBuffer(filepath, &size);
    if (!buf) {
        // Try fallback
        buf = ReadFileToBuffer("../../project.json", &size);
    }
    if (!buf) return false;

    yyjson_alc alc = { YyjsonMalloc, YyjsonRealloc, YyjsonFree, NULL };
    yyjson_doc* doc = yyjson_read_opts(buf, size, YYJSON_READ_INSITU, &alc, NULL);
    if (!doc) { free(buf); return false; }

    yyjson_val* root = yyjson_doc_get_root(doc);
    ScryPlugin_LoadPlugins(ctx);

    yyjson_val* plugins = yyjson_obj_get(root, "plugins");
    if (plugins && yyjson_is_arr(plugins)) {
        size_t i, m; yyjson_val* v;
        yyjson_arr_foreach(plugins, i, m, v) {
            const char* name = yyjson_get_str(v);
            if (name) ScryPlugin_LoadSinglePlugin(ctx, name);
        }
    }

    yyjson_doc_free(doc);
    free(buf);
    return true;
}
