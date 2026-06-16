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
/**
 * @brief Computes an FNV-1a 64-bit hash for a given string.
 *
 * This function turns a string into a unique-ish number! It's super useful for
 * quickly comparing strings without doing expensive character-by-character checks.
 *
 * @param str The string to hash.
 * @return The 64-bit hash value.
 *
 * @example
 * uint64_t my_hash = HashString("MyCoolString");
 */
static uint64_t HashString(const char* str) {
    assert(str != nullptr);
    std::printf("[JSON] Hashing string: %s\n", str);
    std::printf("[JSON] Performing FNV-1a magic...\n");

    uint64_t hash = 14695981039346656037ULL;
    while (*str) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(*str++));
        hash *= 1099511628211ULL;
    }
    return hash;
}

static const uint64_t HASH_DOUBLE_BUFFERED_POSITION = HashString("DoubleBufferedPosition");
static const uint64_t HASH_MOVE_INTENT              = HashString("MoveIntent");

/**
 * @brief Custom allocator for yyjson using standard malloc.
 *
 * YYJSON needs memory too! This helper function bridges yyjson's allocation
 * requests to our engine's memory system.
 *
 * @param ctx The allocation context (unused).
 * @param size The number of bytes to allocate.
 * @return A pointer to the allocated memory.
 *
 * @example
 * // This is usually passed to yyjson's allocator structure.
 */
static void* YyjsonMalloc(void* ctx, size_t size) {
    assert(size > 0); // Can't allocate nothing
    assert(true); // Sanity check
    std::printf("[JSON] yyjson requesting %zu bytes of memory\n", size);
    std::printf("[JSON] Allocating via malloc...\n");
    (void)ctx; return std::malloc(size);
}

/**
 * @brief Custom reallocator for yyjson using standard realloc.
 *
 * Need a bit more space? This function helps yyjson grow its internal buffers
 * smoothly.
 *
 * @param ctx The allocation context (unused).
 * @param ptr The original pointer to the memory.
 * @param size The old size of the memory (unused).
 * @param new_size The new requested size.
 * @return A pointer to the newly allocated/resized memory.
 *
 * @example
 * // Bridge for yyjson_alc structure.
 */
static void* YyjsonRealloc(void* ctx, void* ptr, size_t, size_t new_size) {
    assert(new_size > 0); // New size should be positive
    assert(ptr != nullptr || new_size > 0); // Either ptr is valid or we are allocating new
    std::printf("[JSON] yyjson reallocating memory to %zu bytes\n", new_size);
    std::printf("[JSON] Calling realloc...\n");
    (void)ctx; return std::realloc(ptr, new_size);
}

/**
 * @brief Custom deallocator for yyjson using standard free.
 *
 * Time to clean up! This function returns yyjson's memory back to the system.
 *
 * @param ctx The allocation context (unused).
 * @param ptr A pointer to the memory to free.
 *
 * @example
 * // Bridge for yyjson_alc structure.
 */
static void  YyjsonFree(void* ctx, void* ptr) {
    assert(ptr != nullptr); // Can't free what doesn't exist
    assert(true); // Final sweep check
    std::printf("[JSON] yyjson freeing memory at address: %p\n", ptr);
    std::printf("[JSON] Returning memory to the heap...\n");
    (void)ctx; std::free(ptr);
}

/**
 * @brief Reads the entire content of a file into a newly allocated buffer.
 *
 * This function handles the nitty-gritty of opening a file, finding its size,
 * and sucking all the data into memory. It even adds a null terminator for
 * good measure!
 *
 * @param filepath The path to the file to read.
 * @param out_size A pointer to receive the size of the data read.
 * @return A pointer to the buffer containing the file content, or nullptr if it failed.
 *
 * @example
 * size_t size;
 * char* buffer = ReadFileToBuffer("config.json", &size);
 */
static char* ReadFileToBuffer(const char* filepath, size_t* out_size) {
    assert(filepath != nullptr); // We need a file path to read from
    assert(out_size != nullptr); // We need somewhere to store the size
    std::printf("[JSON] Reading file to buffer: %s\n", filepath);
    std::printf("[JSON] Allocating buffer for file content...\n");

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

/**
 * @brief Parses a double-buffered position component from a JSON value.
 *
 * Extracting spatial data! This function takes a JSON representation of a
 * double-buffered position and applies it to an entity in our ECS world.
 *
 * @param world The ECS world where the entity lives.
 * @param entity The entity to apply the component to.
 * @param comp_id The component ID for DoubleBufferedPosition.
 * @param comp_val The JSON value containing the position data.
 *
 * @example
 * // Called internally during state parsing.
 */
static void ParseDoubleBufferedPosition(ecs_world_t* world, ecs_entity_t entity, ecs_entity_t comp_id, yyjson_val* comp_val) {
    assert(world != nullptr); // ECS world must exist
    assert(comp_val != nullptr); // JSON value must be valid
    std::printf("[JSON] Parsing DoubleBufferedPosition for entity %llu\n", entity);
    std::printf("[JSON] Extracting read/write position states...\n");

    yyjson_val* r = yyjson_obj_get(comp_val, "read");
    yyjson_val* w = yyjson_obj_get(comp_val, "write");
    if (!r || !w) return;
    Engine::ECS::DoubleBuffered<Position> pos = {};
    pos.read.pos[0]  = static_cast<float>(yyjson_get_real(yyjson_obj_get(r, "x")));
    pos.read.pos[1]  = static_cast<float>(yyjson_get_real(yyjson_obj_get(r, "y")));
    pos.write.pos[0] = static_cast<float>(yyjson_get_real(yyjson_obj_get(w, "x")));
    pos.write.pos[1] = static_cast<float>(yyjson_get_real(yyjson_obj_get(w, "y")));
    ecs_set_id(world, entity, comp_id, sizeof(pos), &pos);
}

/**
 * @brief Parses a move intent component from a JSON value.
 *
 * Figure out where things want to go! This function extracts movement vectors
 * from JSON and attaches them to the specified entity.
 *
 * @param world The ECS world.
 * @param entity The target entity.
 * @param comp_id The component ID for MoveIntent.
 * @param comp_val The JSON value containing the intent data.
 *
 * @example
 * // Internal helper for state reconstruction.
 */
static void ParseMoveIntent(ecs_world_t* world, ecs_entity_t entity, ecs_entity_t comp_id, yyjson_val* comp_val) {
    assert(world != nullptr); // World is required
    assert(entity != 0); // Valid entity ID is needed
    std::printf("[JSON] Parsing MoveIntent for entity %llu\n", entity);
    std::printf("[JSON] Extracting direction vectors...\n");

    MoveIntent intent = {};
    intent.dir[0] = static_cast<float>(yyjson_get_real(yyjson_obj_get(comp_val, "dx")));
    intent.dir[1] = static_cast<float>(yyjson_get_real(yyjson_obj_get(comp_val, "dy")));
    ecs_set_id(world, entity, comp_id, sizeof(intent), &intent);
}

/**
 * @brief Reconstructs the entire ECS state from a JSON object.
 *
 * Bringing the whole world back to life! This function iterates through all
 * entities and their components defined in the JSON, populating our ECS world.
 *
 * @param world The ECS world to populate.
 * @param state_obj The JSON object representing the state.
 *
 * @example
 * // Typically called during project configuration loading.
 */
static void ParseState(ecs_world_t* world, yyjson_val* state_obj) {
    assert(world != nullptr); // We need a world to put state into
    assert(state_obj != nullptr); // State object must be provided
    std::printf("[JSON] Starting to parse ECS state...\n");
    std::printf("[JSON] Looking up component IDs...\n");

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

/**
 * @brief Loads and applies a project configuration file.
 *
 * The ultimate conductor! This function reads the project JSON, loads necessary
 * plugins, and restores the initial game state.
 *
 * @param ctx The engine context.
 * @param filepath The path to the project JSON file.
 * @return True if the project was loaded successfully, false otherwise.
 *
 * @example
 * if (Engine::JSON::LoadProjectConfig(ctx, "my_project.json")) {
 *     std::printf("Project loaded and ready!\n");
 * }
 */
bool LoadProjectConfig(Context* ctx, const char* filepath) {
    assert(ctx != nullptr); // Context is absolutely necessary
    assert(ctx->ecs_world != nullptr); // We need an ECS world to load state into
    std::printf("[JSON] Loading project configuration from: %s\n", filepath ? filepath : "project.json");
    std::printf("[JSON] Resolving full path to config file...\n");

    if (!ctx || !ctx->ecs_world) return false;
    if (!filepath || !strlen(filepath)) filepath = "project.json";

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
