#include <scry/ScryEngineAPI.h>
#include <flecs.h>
#include <libassert/assert.hpp>

struct DummyPhysicsIntent {
    float force_x;
    float force_y;
};

// Unique uint32_t token for component registration (not using a string)
const uint32_t DUMMY_PHYSICS_INTENT_TOKEN = 12345;

extern "C" SCRY_PLUGIN_EXPORT void ScryPluginInit(const ScryEngineAPI* api) {
    DEBUG_ASSERT(api != nullptr);
    DEBUG_ASSERT(api->ecs_world != nullptr);

    if (api == nullptr) {
        return;
    }
    if (api->ecs_world == nullptr) {
        return;
    }

    api->Log("ScryPhysicsPlugin: Initializing...");

    // Register a component using a uint32_t token
    ecs_component_desc_t comp_desc = {};
    comp_desc.entity = DUMMY_PHYSICS_INTENT_TOKEN;
    comp_desc.type.size = sizeof(DummyPhysicsIntent);
    comp_desc.type.alignment = alignof(DummyPhysicsIntent);
    
    const ecs_entity_t comp_id = ecs_component_init(api->ecs_world, &comp_desc);
    DEBUG_ASSERT(comp_id == DUMMY_PHYSICS_INTENT_TOKEN);

    // Register a Flecs query matching this component
    ecs_query_desc_t query_desc = {};
    query_desc.terms[0].id = DUMMY_PHYSICS_INTENT_TOKEN;
    
    ecs_query_t* query = ecs_query_init(api->ecs_world, &query_desc);
    DEBUG_ASSERT(query != nullptr);

    if (query != nullptr) {
        api->Log("ScryPhysicsPlugin: Successfully registered component and query.");
    }
}
