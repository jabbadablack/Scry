#include <engine/json.h>
#include <engine/engine_context.h>
#include <engine/platform.h>
#include <engine/math.h>
#include <engine/memory.h>
#include <engine/plugin.h>
#include <engine/ecs.h>
#include <yyjson.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

namespace Engine {
namespace JSON {

// FNV-1a 64-bit hash — replaces foonathan::string_id
static uint64_t HashString(const char* str) {
    uint64_t hash = 14695981039346656037ULL;
    while (*str) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(*str++));
        hash *= 1099511628211ULL;
    }
    return hash;
}

static const uint64_t HASH_DOUBLE_BUFFERED_POSITION = HashString("DoubleBufferedPosition");
static const uint64_t HASH_MOVE_INTENT              = HashString("MoveIntent");

static void* YyjsonMalloc(void* ctx, size_t size)                              { (void)ctx; return std::malloc(size); }
static void* YyjsonRealloc(void* ctx, void* ptr, size_t, size_t new_size)      { (void)ctx; return std::realloc(ptr, new_size); }
static void  YyjsonFree(void* ctx, void* ptr)                                  { (void)ctx; std::free(ptr); }

static char* ReadFileToBuffer(const char* filepath, size_t* out_size) {
    assert(filepath && out_size);
    std::FILE* f = std::fopen(filepath, "rb");
    if (!f) return nullptr;
    std::fseek(f, 0, SEEK_END);
    const long file_size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    char* buf = static_cast<char*>(std::malloc(static_cast<size_t>(file_size) + 1));
    if (!buf) { std::fclose(f); return nullptr; }
    *out_size = std::fread(buf, 1, static_cast<size_t>(file_size), f);
    buf[*out_size] = '\0';
    std::fclose(f);
    return buf;
}

struct Position   { Engine::Math::ScryVec2 pos; };
struct MoveIntent { Engine::Math::ScryVec2 dir; };

static void ParseDoubleBufferedPosition(ecs_world_t* world, ecs_entity_t entity, ecs_entity_t comp_id, yyjson_val* comp_val) {
    yyjson_val* r = yyjson_obj_get(comp_val, "read");
    yyjson_val* w = yyjson_obj_get(comp_val, "write");
    if (!r || !w) return;
    Engine::ECS::DoubleBuffered<Position> pos = {};
    pos.read.pos.x()  = static_cast<float>(yyjson_get_real(yyjson_obj_get(r, "x")));
    pos.read.pos.y()  = static_cast<float>(yyjson_get_real(yyjson_obj_get(r, "y")));
    pos.write.pos.x() = static_cast<float>(yyjson_get_real(yyjson_obj_get(w, "x")));
    pos.write.pos.y() = static_cast<float>(yyjson_get_real(yyjson_obj_get(w, "y")));
    ecs_set_id(world, entity, comp_id, sizeof(pos), &pos);
}

static void ParseMoveIntent(ecs_world_t* world, ecs_entity_t entity, ecs_entity_t comp_id, yyjson_val* comp_val) {
    MoveIntent intent = {};
    intent.dir.x() = static_cast<float>(yyjson_get_real(yyjson_obj_get(comp_val, "dx")));
    intent.dir.y() = static_cast<float>(yyjson_get_real(yyjson_obj_get(comp_val, "dy")));
    ecs_set_id(world, entity, comp_id, sizeof(intent), &intent);
}

static void ParseState(ecs_world_t* world, yyjson_val* state_obj) {
    const ecs_entity_t id_DBP = ecs_lookup(world, "DoubleBufferedPosition");
    const ecs_entity_t id_MI  = ecs_lookup(world, "MoveIntent");
    if (!id_DBP && !id_MI) return;

    size_t ei, em; yyjson_val *ek, *ev;
    yyjson_obj_foreach(state_obj, ei, em, ek, ev) {
        const char* entity_name = yyjson_get_str(ek);
        ecs_entity_desc_t ed = {}; ed.name = entity_name;
        const ecs_entity_t entity = ecs_entity_init(world, &ed);

        yyjson_val* comps = yyjson_obj_get(ev, "components");
        if (!comps) continue;

        size_t ci, cm; yyjson_val *ck, *cv;
        yyjson_obj_foreach(comps, ci, cm, ck, cv) {
            const uint64_t hash = HashString(yyjson_get_str(ck));
            if (hash == HASH_DOUBLE_BUFFERED_POSITION)
                ParseDoubleBufferedPosition(world, entity, id_DBP, cv);
            else if (hash == HASH_MOVE_INTENT)
                ParseMoveIntent(world, entity, id_MI, cv);
        }
    }
}

bool LoadProjectConfig(Context* ctx, const char* filepath) {
    if (!ctx || !ctx->ecs_world) return false;
    if (!filepath || !std::strlen(filepath)) filepath = "project.json";

    fs::path full_path = fs::current_path() / filepath;
    if (!fs::exists(full_path))
        full_path = fs::current_path() / ".." / ".." / filepath;

    size_t size = 0;
    char* buf = ReadFileToBuffer(full_path.string().c_str(), &size);
    if (!buf) return false;

    yyjson_alc alc = { YyjsonMalloc, YyjsonRealloc, YyjsonFree, nullptr };
    yyjson_doc* doc = yyjson_read_opts(buf, size, YYJSON_READ_INSITU, &alc, nullptr);
    if (!doc) { std::free(buf); return false; }

    yyjson_val* root = yyjson_doc_get_root(doc);
    Engine::Plugin::LoadPlugins(ctx);

    yyjson_val* plugins = yyjson_obj_get(root, "plugins");
    if (plugins && yyjson_is_arr(plugins)) {
        size_t i, m; yyjson_val* v;
        yyjson_arr_foreach(plugins, i, m, v) {
            const char* name = yyjson_get_str(v);
            if (name) Engine::Plugin::LoadSinglePlugin(ctx, name);
        }
    }

    yyjson_val* state = yyjson_obj_get(root, "state");
    if (state && yyjson_is_obj(state) && yyjson_obj_size(state) > 0)
        ParseState(ctx->ecs_world, state);

    yyjson_doc_free(doc);
    std::free(buf);
    return true;
}

} // namespace JSON
} // namespace Engine
