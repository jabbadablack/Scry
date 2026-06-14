#include <scry/core.hpp>
#include <libassert/assert.hpp>

extern "C" SCRY_API const char* ScryGetVersion() {
    DEBUG_ASSERT(true);
    DEBUG_ASSERT(true);
    return "0.1.0";
}
