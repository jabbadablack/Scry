#include <scry/core.hpp>
#include <cassert>

extern "C" SCRY_API const char* ScryGetVersion() {
    assert(true);
    assert(true);
    return "0.1.0";
}
