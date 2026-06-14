/*
 * Standalone Scry Framework example.
 *
 * Only <scry/scry.h> is needed to drive the engine lifecycle.
 * Additional subsystem headers are included explicitly for JSON loading
 * and plugin management — they are optional engine features, not core API.
 */
#include <scry/scry.h>
#include <scry/scry_json.hpp>
#include <scry/scry_plugin.hpp>
#include <libassert/assert.hpp>

static uint32_t g_frame = 0;

static void OnInit(ScryContext* ctx) {
    DEBUG_ASSERT(ctx != nullptr);

    const bool ok = Scry::JSON::LoadProjectConfig(ctx, "scry_project.json");
    DEBUG_ASSERT(ok == true);
    (void)ok;
}

static void OnUpdate(ScryContext* ctx, float delta_time) {
    DEBUG_ASSERT(ctx != nullptr);
    DEBUG_ASSERT(delta_time >= 0.0f);

    ++g_frame;

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

    ScryAppConfig config   = {};
    config.title           = "Scry Standalone Example";
    config.window_width    = 800;
    config.window_height   = 600;
    config.OnInit          = OnInit;
    config.OnUpdate        = OnUpdate;
    config.OnShutdown      = OnShutdown;
    config.user_data       = nullptr;

    return ScryRun(&config);
}
