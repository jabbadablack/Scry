#ifndef ENGINE_TIME_TIME_MANAGER_INL
#define ENGINE_TIME_TIME_MANAGER_INL

#include "../debug/assert.h"

#if defined(ENGINE_PLATFORM_WINDOWS)
extern "C" {
    __declspec(dllimport) int __stdcall QueryPerformanceCounter(engine::i64* lpPerformanceCount);
    __declspec(dllimport) int __stdcall QueryPerformanceFrequency(engine::i64* lpFrequency);
    __declspec(dllimport) void __stdcall Sleep(engine::u32 dwMilliseconds);
}
#elif defined(ENGINE_PLATFORM_LINUX)
extern "C" {
    int clock_gettime(int clk_id, void* tp);
    int usleep(engine::u32 usec);
}
#endif

namespace engine {

    ENGINE_INLINE void ITime::Initialize() {
        #if defined(ENGINE_PLATFORM_WINDOWS)
            i64 freq = 0;
            QueryPerformanceFrequency(&freq);
            m_frequency = static_cast<u64>(freq);
        #elif defined(ENGINE_PLATFORM_LINUX)
            m_frequency = 1000000ULL;
        #endif

        ENGINE_ASSERT(m_frequency > 0, "ITime: performance frequency must be greater than zero");

        m_startupTime  = GetOSMicroseconds();
        m_previousTime = m_startupTime;
    }

    ENGINE_INLINE void ITime::Update() {
        u64 current = GetOSMicroseconds();
        u64 delta   = current - m_previousTime;

        m_deltaTime = static_cast<f64>(delta) / 1000000.0;
        if (m_deltaTime > 0.1) {
            m_deltaTime = 0.1;
        }

        m_totalTime    = static_cast<f64>(current - m_startupTime) / 1000000.0;
        m_previousTime = current;
    }

    [[nodiscard]] ENGINE_INLINE f64 ITime::GetDeltaTime() const noexcept {
        return m_deltaTime;
    }

    [[nodiscard]] ENGINE_INLINE f64 ITime::GetTotalTime() const noexcept {
        return m_totalTime;
    }

    [[nodiscard]] ENGINE_INLINE f64 ITime::GetFixedDeltaTime() const noexcept {
        return m_fixedDeltaTime;
    }

    ENGINE_INLINE void ITime::Sleep(u32 milliseconds) noexcept {
        #if defined(ENGINE_PLATFORM_WINDOWS)
            ::Sleep(milliseconds);
        #elif defined(ENGINE_PLATFORM_LINUX)
            ::usleep(milliseconds * 1000);
        #endif
    }

    [[nodiscard]] ENGINE_INLINE u64 ITime::GetOSMicroseconds() const noexcept {
        #if defined(ENGINE_PLATFORM_WINDOWS)
            i64 count = 0;
            QueryPerformanceCounter(&count);
            return (static_cast<u64>(count) * 1000000ULL) / m_frequency;
        #elif defined(ENGINE_PLATFORM_LINUX)
            struct LocalTimeSpec {
                long tv_sec;
                long tv_nsec;
            } ts;
            clock_gettime(1, &ts); // 1 = CLOCK_MONOTONIC
            return (static_cast<u64>(ts.tv_sec) * 1000000ULL) + (static_cast<u64>(ts.tv_nsec) / 1000ULL);
        #endif
    }

} // namespace engine

#endif // ENGINE_TIME_TIME_MANAGER_INL
