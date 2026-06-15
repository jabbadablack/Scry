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
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <vector>
#include <string>

namespace fs = std::filesystem;

// ── Helpers ───────────────────────────────────────────────────────────────────

static bool ensure_dir(const fs::path& p) {
    std::error_code ec;
    fs::create_directories(p, ec);
    return !ec;
}

static fs::path swap_ext(const fs::path& src, const char* new_ext) {
    return src.stem().concat(new_ext);
}

// ── Mesh cooking ──────────────────────────────────────────────────────────────

static bool cook_mesh(const fs::path& input, const fs::path& out_dir) {
    Assimp::Importer imp;
    const aiScene* scene = imp.ReadFile(
        input.string().c_str(),
        aiProcess_Triangulate            |
        aiProcess_JoinIdenticalVertices  |
        aiProcess_GenSmoothNormals       |
        aiProcess_CalcTangentSpace       |
        aiProcess_ConvertToLeftHanded    |   // fixes winding order + UV flips for BGFX
        aiProcess_PreTransformVertices);     // bakes root-node rotation, fixing sideways models

    if (!scene || !scene->HasMeshes()) {
        std::fprintf(stderr, "[cooker] ERROR: assimp failed on %s: %s\n",
            input.string().c_str(), imp.GetErrorString());
        return false;
    }

    /* Flatten all meshes in the scene into a single vertex + index list. */
    std::vector<ScryVertex> verts;
    std::vector<uint32_t>   idxs;

    for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* mesh = scene->mMeshes[m];
        const uint32_t base = static_cast<uint32_t>(verts.size());

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
            verts.push_back(sv);
        }

        for (unsigned f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            for (unsigned i = 0; i < face.mNumIndices; ++i) {
                idxs.push_back(base + face.mIndices[i]);
            }
        }
    }

    if (verts.empty() || idxs.empty()) {
        std::fprintf(stderr, "[cooker] WARN: %s produced no geometry — skipped\n",
            input.string().c_str());
        return false;
    }

    /* ── Deduplication ── */
    {
        // generateVertexRemap strips duplicate verts Assimp leaves behind
        std::vector<unsigned int> remap(idxs.size());
        size_t total_verts = meshopt_generateVertexRemap(
            remap.data(),
            idxs.data(), idxs.size(),
            verts.data(), verts.size(), sizeof(ScryVertex));

        std::vector<ScryVertex> dedup_verts(total_verts);
        std::vector<uint32_t>   dedup_idxs(idxs.size());
        meshopt_remapIndexBuffer (dedup_idxs.data(),  idxs.data(),  idxs.size(),  remap.data());
        meshopt_remapVertexBuffer(dedup_verts.data(), verts.data(), verts.size(), sizeof(ScryVertex), remap.data());

        verts = std::move(dedup_verts);
        idxs  = std::move(dedup_idxs);
    }

    /* ── Optimization ── */
    {
        meshopt_optimizeVertexCache(idxs.data(), idxs.data(), idxs.size(), verts.size());

        std::vector<ScryVertex> optimized_verts(verts.size());
        meshopt_optimizeVertexFetch(optimized_verts.data(), idxs.data(), idxs.size(),
            verts.data(), verts.size(), sizeof(ScryVertex));
        verts = std::move(optimized_verts);
    }

    /* Write .scrymesh */
    const fs::path out = out_dir / swap_ext(input, ".scrymesh");
    FILE* f = std::fopen(out.string().c_str(), "wb");
    if (!f) {
        std::fprintf(stderr, "[cooker] ERROR: cannot create %s\n", out.string().c_str());
        return false;
    }

    ScryMeshHeader hdr{};
    hdr.magic        = SCRY_MESH_MAGIC;
    hdr.version      = SCRY_MESH_VERSION;
    hdr.vertex_count = static_cast<uint32_t>(verts.size());
    hdr.index_count  = static_cast<uint32_t>(idxs.size());

    std::fwrite(&hdr,        sizeof(hdr),               1,           f);
    std::fwrite(verts.data(), sizeof(ScryVertex),  verts.size(), f);
    std::fwrite(idxs.data(),  sizeof(uint32_t),    idxs.size(),  f);
    std::fclose(f);

    std::printf("[cooker] %s -> %s  (v=%u i=%u)\n",
        input.filename().string().c_str(),
        out.filename().string().c_str(),
        hdr.vertex_count, hdr.index_count);
    return true;
}

// ── Texture cooking ───────────────────────────────────────────────────────────

static bool cook_texture(const fs::path& input, const fs::path& out_dir) {
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

int main(int argc, char* argv[]) {
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
