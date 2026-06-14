#include <engine/PluginAPI.h>
#include <flecs.h>
#include <libassert/assert.hpp>

// Simulated raw key event pushed into the world by this plugin.
struct SimulatedKeyEvent {
    uint32_t scancode;
    uint8_t  pressed;
    uint8_t  _pad[3];
};

static const uint32_t SIMULATED_KEY_EVENT_TOKEN = 98765;

extern "C" PLUGIN_EXPORT void PluginInit(const PluginAPI* api) {
    DEBUG_ASSERT(api != nullptr);
    DEBUG_ASSERT(api->ecs_world != nullptr);

    if (api == nullptr || api->ecs_world == nullptr) {
        return;
    }

    api->Log("ScryInputPlugin: Initializing mock input plugin.");

    ecs_component_desc_t comp_desc = {};
    comp_desc.entity    = SIMULATED_KEY_EVENT_TOKEN;
    comp_desc.type.size      = sizeof(SimulatedKeyEvent);
    comp_desc.type.alignment = alignof(SimulatedKeyEvent);

    const ecs_entity_t comp_id = ecs_component_init(api->ecs_world, &comp_desc);
    DEBUG_ASSERT(comp_id == SIMULATED_KEY_EVENT_TOKEN);
    (void)comp_id;

    api->Log("ScryInputPlugin: SimulatedKeyEvent component registered. Mock input routing active.");
}
