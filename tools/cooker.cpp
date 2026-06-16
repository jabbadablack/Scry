/* scry_cooker — offline asset pipeline
 * Usage: scry_cooker <input_dir> <output_dir>
 *
 * Processes every .gltf / .glb / .obj / .fbx in input_dir into a .scrymesh binary,
 * and every .png into a .scrytex binary.  Writes into output_dir (created if
 * it does not exist).  Links ONLY against assimp + stb + meshoptimizer — no Scry runtime.
 */

#include <engine/CookedAsset.h>

/* ── MeshOptimizer ───────────────────────────────────────────────────────── */
#include <meshoptimizer.h>

/* ── Assimp ──────────────────────────────────────────────────────────────── */
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

/* ── STB Image ───────────────────────────────────────────────────────────── */
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

/* ── Standard library ────────────────────────────────────────────────────── */
#include <cstdio>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <vector>
#include <string>

namespace fs = std::filesystem;

// ── Helpers ───────────────────────────────────────────────────────────────────

/**
 * @brief Just making sure we have a place to put things! This creates directories if they're missing.
 * 
 * @param p The path you want to make sure exists.
 * @return True if everything is ready, false if we hit a snag.
 * 
 * @example
 * ensure_dir("assets/cooked");
 */
static bool ensure_dir(const fs::path& p) {
    assert(!p.empty() && "Oops! Can't ensure an empty path.");
    assert(p.is_absolute() || p.is_relative());
    std::printf("[cooker] Making sure directory exists: %s\n", p.string().c_str());
    std::printf("[cooker] Checking for directories now...\n");
    std::error_code ec;
    fs::create_directories(p, ec);
    return !ec;
}

/**
 * @brief Need a new look? This swaps the file extension for you.
 * 
 * @param src The original file path.
 * @param new_ext The shiny new extension you want (don't forget the dot!).
 * @return A new path with the new extension.
 * 
 * @example
 * fs::path my_file = swap_ext("model.obj", ".scrymesh");
 */
static fs::path swap_ext(const fs::path& src, const char* new_ext) {
    assert(new_ext != nullptr && "Don't forget the extension!");
    assert(new_ext[0] == '.' && "Extension should start with a dot, like '.txt'");
    std::printf("[cooker] Swapping extension of %s to %s\n", src.string().c_str(), new_ext);
    std::printf("[cooker] Concatenating new extension...\n");
    return src.stem().concat(new_ext);
}

// ── Mesh cooking ──────────────────────────────────────────────────────────────

/**
 * @brief Let's get cooking! This function turns your 3D models into optimized .scrymesh files.
 * 
 * It handles all the heavy lifting like deduplicating vertices and generating LODs.
 * 
 * @param input The path to your raw model file (GLTF, OBJ, etc.).
 * @param out_dir Where you want the shiny new .scrymesh file to live.
 * @return True if the mesh was cooked to perfection, false otherwise.
 * 
 * @example
 * cook_mesh("assets/raw/suzanne.obj", "assets/cooked");
 */
static bool cook_mesh(const fs::path& input, const fs::path& out_dir) {
    assert(!input.empty() && "Wait, where's the input file?");
    assert(!out_dir.empty() && "We need somewhere to put the cooked mesh!");
    std::printf("[cooker] Cooking mesh: %s\n", input.string().c_str());
    std::printf("[cooker] Getting the Assimp importer ready...\n");
    Assimp::Importer imp;
    const aiScene* scene = imp.ReadFile(
        input.string().c_str(),
        aiProcess_Triangulate           |
        aiProcess_JoinIdenticalVertices |
        aiProcess_PreTransformVertices  |  // bakes node hierarchy, fixes sideways FBX models
        aiProcess_ImproveCacheLocality  |
        aiProcess_SortByPType);            // isolates triangle meshes from lines/points

    if (!scene || !scene->HasMeshes()) {
        std::fprintf(stderr, "[cooker] ERROR: assimp failed on %s: %s\n",
            input.string().c_str(), imp.GetErrorString());
        return false;
    }

    // ── Step 1: Raw extraction — combine all sub-meshes with index offsets ───
    std::vector<ScryVertex> unoptimized_verts;
    std::vector<uint32_t>   combined_idxs;

    for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* mesh = scene->mMeshes[m];
        if (!(mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE)) continue;

        const uint32_t index_offset = static_cast<uint32_t>(unoptimized_verts.size());

        for (unsigned v = 0; v < mesh->mNumVertices; ++v) {
            ScryVertex sv{};
            sv.px = mesh->mVertices[v].x;
            sv.py = mesh->mVertices[v].y;
            sv.pz = mesh->mVertices[v].z;
            if (mesh->HasNormals()) {
                sv.nx = mesh->mNormals[v].x;
                sv.ny = mesh->mNormals[v].y;
                sv.nz = mesh->mNormals[v].z;
            }
            if (mesh->HasTextureCoords(0)) {
                sv.u = mesh->mTextureCoords[0][v].x;
                sv.v = mesh->mTextureCoords[0][v].y;
            }
            unoptimized_verts.push_back(sv);
        }

        for (unsigned f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            for (unsigned j = 0; j < face.mNumIndices; ++j)
                combined_idxs.push_back(face.mIndices[j] + index_offset);
        }
    }

    if (unoptimized_verts.empty() || combined_idxs.empty()) {
        std::fprintf(stderr, "[cooker] WARN: %s produced no geometry — skipped\n",
            input.string().c_str());
        return false;
    }

    const size_t raw_vertex_count = unoptimized_verts.size();

    // ── Step 2: Remap pass (deduplication) ───────────────────────────────────
    std::vector<unsigned int> remap(combined_idxs.size());
    const size_t unique_vertices = meshopt_generateVertexRemap(
        remap.data(),
        combined_idxs.data(), combined_idxs.size(),
        unoptimized_verts.data(), unoptimized_verts.size(), sizeof(ScryVertex));

    // ── Step 3: Allocate dense output arrays ─────────────────────────────────
    std::vector<ScryVertex> optimized_verts(unique_vertices);
    std::vector<uint32_t>   optimized_idxs(combined_idxs.size());

    // ── Step 4: Apply remap ──────────────────────────────────────────────────
    meshopt_remapIndexBuffer(
        optimized_idxs.data(),
        combined_idxs.data(), combined_idxs.size(),
        remap.data());
    meshopt_remapVertexBuffer(
        optimized_verts.data(),
        unoptimized_verts.data(), unoptimized_verts.size(), sizeof(ScryVertex),
        remap.data());

    // ── Step 5: Cache-optimize — LOD0 is the full deduplicated + cache-tuned mesh ─
    meshopt_optimizeVertexCache(
        optimized_idxs.data(), optimized_idxs.data(),
        optimized_idxs.size(), optimized_verts.size());

    std::vector<uint32_t> lod0_idxs = optimized_idxs; // LOD0 = full mesh

    // ── Step 6: LOD1 — 50% of LOD0 triangle count ────────────────────────────
    const size_t lod1_target = (size_t)(lod0_idxs.size() * 0.5f);
    std::vector<uint32_t> lod1_idxs(lod0_idxs.size());
    float lod1_error = 0.0f;
    const size_t lod1_index_count = meshopt_simplify(
        lod1_idxs.data(),
        lod0_idxs.data(), lod0_idxs.size(),
        &optimized_verts[0].px, optimized_verts.size(), sizeof(ScryVertex),
        lod1_target, 0.01f, 0, &lod1_error);
    lod1_idxs.resize(lod1_index_count);
    meshopt_optimizeVertexCache(
        lod1_idxs.data(), lod1_idxs.data(), lod1_idxs.size(), optimized_verts.size());

    // ── Step 7: LOD2 — 10% of LOD0 triangle count ────────────────────────────
    const size_t lod2_target = (size_t)(lod0_idxs.size() * 0.1f);
    std::vector<uint32_t> lod2_idxs(lod0_idxs.size());
    float lod2_error = 0.0f;
    const size_t lod2_index_count = meshopt_simplify(
        lod2_idxs.data(),
        lod0_idxs.data(), lod0_idxs.size(),
        &optimized_verts[0].px, optimized_verts.size(), sizeof(ScryVertex),
        lod2_target, 0.05f, 0, &lod2_error);
    lod2_idxs.resize(lod2_index_count);
    meshopt_optimizeVertexCache(
        lod2_idxs.data(), lod2_idxs.data(), lod2_idxs.size(), optimized_verts.size());

    // ── Step 8: Per-LOD independent vertex extraction ────────────────────────────
    // All three simplifications above ran before any fetch-opt call, so lod1_idxs
    // and lod2_idxs still reference optimized_verts index space here.
    // meshopt_optimizeVertexFetch modifies each index buffer IN PLACE to reference
    // the new compact per-LOD vertex buffer; optimized_verts is never written to.
    std::vector<ScryVertex> lod0_verts(optimized_verts.size());
    size_t lod0_v_count = meshopt_optimizeVertexFetch(
        lod0_verts.data(), lod0_idxs.data(), lod0_idxs.size(),
        optimized_verts.data(), optimized_verts.size(), sizeof(ScryVertex));
    lod0_verts.resize(lod0_v_count);

    std::vector<ScryVertex> lod1_verts(optimized_verts.size());
    size_t lod1_v_count = meshopt_optimizeVertexFetch(
        lod1_verts.data(), lod1_idxs.data(), lod1_idxs.size(),
        optimized_verts.data(), optimized_verts.size(), sizeof(ScryVertex));
    lod1_verts.resize(lod1_v_count);

    std::vector<ScryVertex> lod2_verts(optimized_verts.size());
    size_t lod2_v_count = meshopt_optimizeVertexFetch(
        lod2_verts.data(), lod2_idxs.data(), lod2_idxs.size(),
        optimized_verts.data(), optimized_verts.size(), sizeof(ScryVertex));
    lod2_verts.resize(lod2_v_count);

    // ── Step 9: Write .scrymesh ───────────────────────────────────────────────
    const fs::path out = out_dir / swap_ext(input, ".scrymesh");
    FILE* f = std::fopen(out.string().c_str(), "wb");
    if (!f) {
        std::fprintf(stderr, "[cooker] ERROR: cannot create %s\n", out.string().c_str());
        return false;
    }

    ScryMeshHeader hdr{};
    hdr.magic             = SCRY_MESH_MAGIC;
    hdr.version           = SCRY_MESH_VERSION;
    hdr.lod0_vertex_count = static_cast<uint32_t>(lod0_v_count);
    hdr.lod0_index_count  = static_cast<uint32_t>(lod0_idxs.size());
    hdr.lod1_vertex_count = static_cast<uint32_t>(lod1_v_count);
    hdr.lod1_index_count  = static_cast<uint32_t>(lod1_index_count);
    hdr.lod2_vertex_count = static_cast<uint32_t>(lod2_v_count);
    hdr.lod2_index_count  = static_cast<uint32_t>(lod2_index_count);

    // [Header][LOD0_Verts][LOD0_Idxs][LOD1_Verts][LOD1_Idxs][LOD2_Verts][LOD2_Idxs]
    std::fwrite(&hdr,              sizeof(hdr),        1,                f);
    std::fwrite(lod0_verts.data(), sizeof(ScryVertex), lod0_v_count,     f);
    std::fwrite(lod0_idxs.data(),  sizeof(uint32_t),   lod0_idxs.size(), f);
    std::fwrite(lod1_verts.data(), sizeof(ScryVertex), lod1_v_count,     f);
    std::fwrite(lod1_idxs.data(),  sizeof(uint32_t),   lod1_index_count, f);
    std::fwrite(lod2_verts.data(), sizeof(ScryVertex), lod2_v_count,     f);
    std::fwrite(lod2_idxs.data(),  sizeof(uint32_t),   lod2_index_count, f);
    std::fclose(f);

    std::printf(
        "[cooker] %s -> %s  (%zu raw -> %zu dedup | "
        "LOD0: %zu v / %zu idx | LOD1: %zu v / %zu idx (err=%.4f) | LOD2: %zu v / %zu idx (err=%.4f))\n",
        input.filename().string().c_str(),
        out.filename().string().c_str(),
        raw_vertex_count, unique_vertices,
        lod0_v_count, lod0_idxs.size(),
        lod1_v_count, lod1_index_count, static_cast<double>(lod1_error),
        lod2_v_count, lod2_index_count, static_cast<double>(lod2_error));
    return true;
}

// ── Texture cooking ───────────────────────────────────────────────────────────

/**
 * @brief Time to make some textures! This function turns your PNGs into .scrytex files.
 * 
 * It uses stb_image to load the pixels and then wraps them in a nice Scry header.
 * 
 * @param input The path to your raw PNG file.
 * @param out_dir Where the cooked .scrytex file should go.
 * @return True if the texture was cooked successfully, false otherwise.
 * 
 * @example
 * cook_texture("assets/raw/diffuse.png", "assets/cooked");
 */
static bool cook_texture(const fs::path& input, const fs::path& out_dir) {
    assert(!input.empty() && "We need an input file to cook!");
    assert(!out_dir.empty() && "Where should I put the cooked texture?");
    std::printf("[cooker] Preparing to cook texture: %s\n", input.string().c_str());
    std::printf("[cooker] Loading image data via stb_image...\n");

    int w = 0, h = 0, ch = 0;
    uint8_t* pixels = stbi_load(input.string().c_str(), &w, &h, &ch, 4 /* force RGBA */);
    if (!pixels) {
        std::fprintf(stderr, "[cooker] ERROR: stb_image failed on %s: %s\n",
            input.string().c_str(), stbi_failure_reason());
        return false;
    }

    const fs::path out = out_dir / swap_ext(input, ".scrytex");
    FILE* f = std::fopen(out.string().c_str(), "wb");
    if (!f) {
        std::fprintf(stderr, "[cooker] ERROR: cannot create %s\n", out.string().c_str());
        stbi_image_free(pixels);
        return false;
    }

    ScryTexHeader hdr{};
    hdr.magic   = SCRY_TEX_MAGIC;
    hdr.version = SCRY_TEX_VERSION;
    hdr.width   = static_cast<uint32_t>(w);
    hdr.height  = static_cast<uint32_t>(h);

    std::fwrite(&hdr,   sizeof(hdr),                   1, f);
    std::fwrite(pixels, 1, static_cast<size_t>(w * h * 4), f);
    std::fclose(f);
    stbi_image_free(pixels);

    std::printf("[cooker] %s -> %s  (%dx%d RGBA)\n",
        input.filename().string().c_str(),
        out.filename().string().c_str(), w, h);
    return true;
}

// ── Entry point ───────────────────────────────────────────────────────────────

/**
 * @brief The heart of the cooker! This is where we start processing all your raw assets.
 * 
 * It scans the input directory for meshes and textures, and cooks them all up for the engine.
 * 
 * @param argc The number of command-line arguments.
 * @param argv The command-line arguments (input_dir and output_dir).
 * @return 0 if everything went smoothly, 1 if we encountered some hiccups.
 * 
 * @example
 * // Run from the command line:
 * // scry_cooker assets/raw assets/cooked
 */
int main(int argc, char* argv[]) {
    assert(argv != nullptr && "Arguments shouldn't be null!");
    assert(argc >= 1 && "At least the program name should be present.");
    std::printf("[cooker] Starting the cooking process...\n");
    std::printf("[cooker] Parsing command line arguments...\n");

    if (argc != 3) {
        std::fprintf(stderr, "Usage: scry_cooker <input_dir> <output_dir>\n");
        return 1;
    }

    const fs::path in_dir  = argv[1];
    const fs::path out_dir = argv[2];

    if (!fs::exists(in_dir)) {
        /* Empty raw dir is not an error — just nothing to cook. */
        std::printf("[cooker] Input dir empty or missing: %s — nothing to cook.\n",
            in_dir.string().c_str());
        return 0;
    }

    if (!ensure_dir(out_dir)) {
        std::fprintf(stderr, "[cooker] ERROR: cannot create output dir %s\n",
            out_dir.string().c_str());
        return 1;
    }

    int cooked = 0, failed = 0;
    for (const auto& entry : fs::recursive_directory_iterator(in_dir)) {
        if (!entry.is_regular_file()) continue;
        const fs::path& p   = entry.path();
        const std::string ext = p.extension().string();

        bool ok = false;
        if (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".fbx") {
            ok = cook_mesh(p, out_dir);
        } else if (ext == ".png") {
            ok = cook_texture(p, out_dir);
        } else {
            continue;
        }

        if (ok) ++cooked; else ++failed;
    }

    std::printf("[cooker] Done: %d cooked, %d failed.\n", cooked, failed);
    return (failed > 0) ? 1 : 0;
}
