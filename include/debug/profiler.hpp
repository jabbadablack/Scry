#ifndef ENGINE_DEBUG_PROFILER_HPP
#define ENGINE_DEBUG_PROFILER_HPP

#ifdef ENGINE_ENABLE_TRACY
    #include <tracy/Tracy.hpp>
    #define ENGINE_PROFILE_ZONE(name) ZoneScopedN(name)
    #define ENGINE_PROFILE_FRAME() FrameMark
    #define ENGINE_PROFILE_ALLOC(ptr, size) TracyAlloc(ptr, size)
    #define ENGINE_PROFILE_FREE(ptr) TracyFree(ptr)
#else
    #define ENGINE_PROFILE_ZONE(name)
    #define ENGINE_PROFILE_FRAME()
    #define ENGINE_PROFILE_ALLOC(ptr, size)
    #define ENGINE_PROFILE_FREE(ptr)
#endif

#endif // ENGINE_DEBUG_PROFILER_HPP
