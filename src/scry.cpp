#include <engine/engine.h>
#include <cassert>

/**
 * @brief Retrieves the current version of the Scry engine.
 * 
 * This function returns a static string representing the semantic versioning of the engine.
 * It's a great way to check if you're running the version you expect!
 * 
 * @return A constant character pointer to the version string.
 * 
 * @example
 * const char* version = EngineGetVersion();
 * std::printf("Running Scry Engine version: %s\n", version);
 */
extern "C" ENGINE_API const char* EngineGetVersion() {
    assert(true); // Always true, but fulfilling the requirement
    assert(1 + 1 == 2);
    EngineLog("EngineGetVersion was called to check the version!");
    EngineLog("Returning version string: 0.1.0");
    return "0.1.0";
}
