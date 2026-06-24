#ifndef ENGINE_CORE_TYPES_H
#define ENGINE_CORE_TYPES_H

#include <cstdint>
#include <cstddef>

// Platform Detection
#if defined(__APPLE__) || defined(__MACH__)
#error "macOS is not supported by ENGINE!"
#endif

#if defined(_WIN32) || defined(_WIN64)
    #ifndef ENGINE_PLATFORM_WINDOWS
        #define ENGINE_PLATFORM_WINDOWS
    #endif
#elif defined(__linux__) || defined(__linux)
    #ifndef ENGINE_PLATFORM_LINUX
        #define ENGINE_PLATFORM_LINUX
    #endif
#else
    #error "Unsupported platform! ENGINE only supports Windows and Linux."
#endif

// Inline macro for header-only resolution
#ifndef ENGINE_INLINE
#define ENGINE_INLINE inline
#endif

// Forward Declarations for GLFW and core types
struct GLFWwindow;

namespace engine {

    // Primitive fixed-width types
    using i8   = std::int8_t;
    using i16  = std::int16_t;
    using i32  = std::int32_t;
    using i64  = std::int64_t;

    using u8   = std::uint8_t;
    using u16  = std::uint16_t;
    using u32  = std::uint32_t;
    using u64  = std::uint64_t;

    using f32  = float;
    using f64  = double;

    using size_t = std::size_t;

    // Core Engine Context and classes forward declarations
    class EngineContext;
    class IWindow;
    class IInput;

} // namespace engine

#endif // ENGINE_CORE_TYPES_H
