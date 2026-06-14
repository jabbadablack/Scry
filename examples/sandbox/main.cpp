// Standalone Scry Framework example.
// This file intentionally imports only the public umbrella header — it has no
// knowledge of mimalloc, SDL3, Flecs, or Eigen internals.  Linking against the
// scry shared library is the only build-time dependency.
#include <scry/scry.h>

static uint32_t g_frame = 0;

static void OnInit(ScryContext* ctx) {
    DEBUG_ASSERT(ctx != nullptr);

    // Load runtime plugin manifest.  The CMake JSON parser already built
    // ScryInputPlugin by reading project.json at configure time; here we prove
    // the engine loads it at runtime from the same declaration.
    const bool ok = Scry::JSON::LoadProjectConfig(ctx, "scry_project.json");
    DEBUG_ASSERT(ok == true);
    (void)ok;
}

static void OnUpdate(ScryContext* ctx, float delta_time) {
    DEBUG_ASSERT(ctx != nullptr);
    DEBUG_ASSERT(delta_time >= 0.0f);

    ++g_frame;

    // Auto-exit after a few frames so the example finishes cleanly when run
    // headlessly (e.g. in CI).
    if (g_frame >= 10) {
        RequestEngineExit(ctx);
    }
}

static void OnShutdown(ScryContext* ctx) {
    DEBUG_ASSERT(ctx != nullptr);

    Scry::Plugin::UnloadPlugins();
}

int main(int argc, char* argv[]) {
    DEBUG_ASSERT(argc >= 1);
    DEBUG_ASSERT(argv != nullptr);

    ScryAppConfig config    = {};
    config.OnInit           = OnInit;
    config.OnUpdate         = OnUpdate;
    config.OnShutdown       = OnShutdown;
    config.window_width     = 800;
    config.window_height    = 600;
    config.app_name         = "Scry Standalone Example";

    return ScryRun(&config);
}
