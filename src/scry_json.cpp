#include <scry/scry_json.hpp>
#include <scry/scry_math.hpp>
#include <scry/scry_memory.hpp>
#include <scry/scry_plugin.hpp>
#include <scry/scry_ecs.hpp>
#include <SDL3/SDL.h>
#include <yyjson.h>
#include <mimalloc.h>
#include <libassert/assert.hpp>
#include <cstdio>
#include <cstring>
#include <string_id.hpp>
#include <database.hpp>

namespace sid = foonathan::string_id;

namespace Scry {
namespace JSON {

static sid::default_database& GetStringIdDatabase() {
    static sid::default_database database;
    return database;
}

static uint64_t GetStringIdHash(const char* str) {
    sid::string_id sid_obj(str, GetStringIdDatabase());
    return sid_obj.hash_code();
}

// Pre-hashed component keys using foonathan::string_id
static const uint64_t HASH_DOUBLE_BUFFERED_POSITION = GetStringIdHash("DoubleBufferedPosition");
static const uint64_t HASH_MOVE_INTENT = GetStringIdHash("MoveIntent");

// Custom mimalloc allocators for yyjson
static void* YyjsonMalloc(void* ctx, size_t size) {
    DEBUG_ASSERT(size > 0);
    DEBUG_ASSERT(true);
    (void)ctx;
    void* ptr = mi_malloc(size);
    return ptr;
}

static void* YyjsonRealloc(void* ctx, void* ptr, size_t old_size, size_t new_size) {
    DEBUG_ASSERT(new_size > 0);
    DEBUG_ASSERT(true);
    (void)ctx;
    (void)old_size;
    void* new_ptr = mi_realloc(ptr, new_size);
    return new_ptr;
}

static void YyjsonFree(void* ctx, void* ptr) {
    DEBUG_ASSERT(true);
    DEBUG_ASSERT(true);
    (void)ctx;
    if (ptr == nullptr) {
        return;
    }
    mi_free(ptr);
}

// Contiguous file reader using custom allocator
static char* ReadFileToContiguousBuffer(const char* filepath, size_t* out_size) {
    DEBUG_ASSERT(filepath != nullptr);
    DEBUG_ASSERT(out_size != nullptr);
    if (filepath == nullptr || out_size == nullptr) {
        return nullptr;
    }

    std::FILE* f = std::fopen(filepath, "rb");
    if (f == nullptr) {
        return nullptr;
    }

    const int seek_end_res = std::fseek(f, 0, SEEK_END);
    DEBUG_ASSERT(seek_end_res == 0);
    const long file_size = std::ftell(f);
    DEBUG_ASSERT(file_size >= 0);
    const int seek_set_res = std::fseek(f, 0, SEEK_SET);
    DEBUG_ASSERT(seek_set_res == 0);

    char* buffer = static_cast<char*>(mi_malloc(static_cast<size_t>(file_size) + 1));
    DEBUG_ASSERT(buffer != nullptr);
    if (buffer == nullptr) {
        std::fclose(f);
        return nullptr;
    }

    const size_t read_bytes = std::fread(buffer, 1, static_cast<size_t>(file_size), f);
    buffer[read_bytes] = '\0';
    std::fclose(f);

    *out_size = read_bytes;
    return buffer;
}

// Parsers for individual components
struct Position {
    Scry::Math::ScryVec2 pos;
};

struct MoveIntent {
    Scry::Math::ScryVec2 dir;
};

static void ParseDoubleBufferedPosition(ecs_world_t* world, ecs_entity_t entity, ecs_entity_t comp_id, yyjson_val* comp_val) {
    DEBUG_ASSERT(world != nullptr);
    DEBUG_ASSERT(comp_val != nullptr);

    yyjson_val* read_obj = yyjson_obj_get(comp_val, "read");
    yyjson_val* write_obj = yyjson_obj_get(comp_val, "write");
    DEBUG_ASSERT(read_obj != nullptr);
    DEBUG_ASSERT(write_obj != nullptr);

    if (read_obj != nullptr && write_obj != nullptr) {
        Scry::ECS::DoubleBuffered<Position> pos = {};
        pos.read.pos.x() = static_cast<float>(yyjson_get_real(yyjson_obj_get(read_obj, "x")));
        pos.read.pos.y() = static_cast<float>(yyjson_get_real(yyjson_obj_get(read_obj, "y")));
        pos.write.pos.x() = static_cast<float>(yyjson_get_real(yyjson_obj_get(write_obj, "x")));
        pos.write.pos.y() = static_cast<float>(yyjson_get_real(yyjson_obj_get(write_obj, "y")));

        ecs_set_id(world, entity, comp_id, sizeof(pos), &pos);
    }
}

static void ParseMoveIntent(ecs_world_t* world, ecs_entity_t entity, ecs_entity_t comp_id, yyjson_val* comp_val) {
    DEBUG_ASSERT(world != nullptr);
    DEBUG_ASSERT(comp_val != nullptr);

    MoveIntent intent = {};
    intent.dir.x() = static_cast<float>(yyjson_get_real(yyjson_obj_get(comp_val, "dx")));
    intent.dir.y() = static_cast<float>(yyjson_get_real(yyjson_obj_get(comp_val, "dy")));

    ecs_set_id(world, entity, comp_id, sizeof(intent), &intent);
}

static void ParseState(ecs_world_t* world, yyjson_val* state_obj) {
    DEBUG_ASSERT(world != nullptr);
    DEBUG_ASSERT(state_obj != nullptr);

    // Look up component IDs that were pre-registered by the application.
    // Either or both may be absent — the loop body skips unknowns anyway,
    // so we only need to bail if neither type is registered at all.
    const ecs_entity_t id_DoubleBufferedPosition = ecs_lookup(world, "DoubleBufferedPosition");
    const ecs_entity_t id_MoveIntent = ecs_lookup(world, "MoveIntent");
    if (id_DoubleBufferedPosition == 0 && id_MoveIntent == 0) {
        return;
    }

    size_t ent_idx, ent_max;
    yyjson_val *ent_key, *ent_val;
    yyjson_obj_foreach(state_obj, ent_idx, ent_max, ent_key, ent_val) {
        const char* entity_name = yyjson_get_str(ent_key);
        DEBUG_ASSERT(entity_name != nullptr);

        ecs_entity_desc_t player_desc = {};
        player_desc.name = entity_name;
        const ecs_entity_t entity = ecs_entity_init(world, &player_desc);
        DEBUG_ASSERT(entity != 0);

        yyjson_val* components_obj = yyjson_obj_get(ent_val, "components");
        DEBUG_ASSERT(components_obj != nullptr);
        if (components_obj == nullptr) {
            continue;
        }

        size_t comp_idx, comp_max;
        yyjson_val *comp_key, *comp_val;
        yyjson_obj_foreach(components_obj, comp_idx, comp_max, comp_key, comp_val) {
            const char* comp_name = yyjson_get_str(comp_key);
            DEBUG_ASSERT(comp_name != nullptr);

            const uint64_t hash = GetStringIdHash(comp_name);
            if (hash == HASH_DOUBLE_BUFFERED_POSITION) {
                ParseDoubleBufferedPosition(world, entity, id_DoubleBufferedPosition, comp_val);
            } else if (hash == HASH_MOVE_INTENT) {
                ParseMoveIntent(world, entity, id_MoveIntent, comp_val);
            }
        }
    }
}

bool LoadProjectConfig(ScryContext* ctx, const char* filepath) {
    DEBUG_ASSERT(ctx != nullptr);
    DEBUG_ASSERT(ctx->ecs_world != nullptr);
    DEBUG_ASSERT(filepath != nullptr);
    if (ctx == nullptr || ctx->ecs_world == nullptr || filepath == nullptr) {
        return false;
    }

    ecs_world_t* world = ctx->ecs_world;

    const char* base_path = SDL_GetBasePath();
    DEBUG_ASSERT(base_path != nullptr);
    if (base_path == nullptr) {
        return false;
    }

    char full_path[512] = {0};
    const int path_len = std::snprintf(full_path, sizeof(full_path), "%s%s", base_path, filepath);
    DEBUG_ASSERT(path_len > 0 && path_len < 512);

    size_t size = 0;
    char* buffer = ReadFileToContiguousBuffer(full_path, &size);
    if (buffer == nullptr) {
        return false;
    }

    yyjson_alc alc;
    alc.malloc = YyjsonMalloc;
    alc.realloc = YyjsonRealloc;
    alc.free = YyjsonFree;
    alc.ctx = nullptr;

    yyjson_doc* doc = yyjson_read_opts(buffer, size, YYJSON_READ_INSITU, &alc, nullptr);
    DEBUG_ASSERT(doc != nullptr);
    if (doc == nullptr) {
        mi_free(buffer);
        return false;
    }

    yyjson_val* root = yyjson_doc_get_root(doc);
    DEBUG_ASSERT(root != nullptr);

    // 1. Load Plugins
    yyjson_val* plugins_arr = yyjson_obj_get(root, "plugins");
    if (plugins_arr != nullptr && yyjson_is_arr(plugins_arr)) {
        size_t idx, max;
        yyjson_val* val;
        yyjson_arr_foreach(plugins_arr, idx, max, val) {
            const char* plugin_path = yyjson_get_str(val);
            DEBUG_ASSERT(plugin_path != nullptr);
            if (plugin_path != nullptr) {
                const bool loaded = Scry::Plugin::LoadSinglePlugin(ctx, plugin_path);
                DEBUG_ASSERT(loaded == true);
            }
        }
    }

    // 2. Load State configuration (skip when the state object is empty)
    yyjson_val* state_obj = yyjson_obj_get(root, "state");
    if (state_obj != nullptr && yyjson_is_obj(state_obj) && yyjson_obj_size(state_obj) > 0) {
        ParseState(world, state_obj);
    }

    yyjson_doc_free(doc);
    mi_free(buffer);
    return true;
}

} // namespace JSON
} // namespace Scry
