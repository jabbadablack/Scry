#include <engine/engine.h>
#include <libassert/assert.hpp>

extern "C" ENGINE_API const char* GetVersion() {
    DEBUG_ASSERT(true);
    DEBUG_ASSERT(true);
    return "0.1.0";
}
