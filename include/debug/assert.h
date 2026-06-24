#ifndef ENGINE_CORE_ASSERT_H
#define ENGINE_CORE_ASSERT_H

#include "../OS/types.h"

#ifndef NDEBUG
    #include <iostream>

    #if defined(ENGINE_PLATFORM_WINDOWS)
        #if defined(__cplusplus)
            extern "C" {
        #endif
        // Declare __debugbreak or DebugBreak for MSVC / MinGW Windows compatibility
        void __cdecl __debugbreak(void);
        #if defined(__cplusplus)
            }
        #endif
        #define ENGINE_DEBUG_BREAK() __debugbreak()
    #elif defined(ENGINE_PLATFORM_LINUX)
        #define ENGINE_DEBUG_BREAK() __builtin_trap()
    #else
        #define ENGINE_DEBUG_BREAK() ((void)0)
    #endif

    #define ENGINE_ASSERT(condition, message) \
        do { \
            if (!(condition)) { \
                std::cerr << "[ENGINE ASSERTION FAILED] File: " << __FILE__ \
                          << ", Line: " << __LINE__ \
                          << "\nExpression: " << #condition \
                          << "\nMessage: " << (message) << "\n"; \
                ENGINE_DEBUG_BREAK(); \
            } \
        } while (0)
#else
    #define ENGINE_ASSERT(condition, message) do { (void)(condition); (void)(message); } while (0)
#endif

#endif // ENGINE_CORE_ASSERT_H
