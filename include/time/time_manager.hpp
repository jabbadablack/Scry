#ifndef ENGINE_TIME_TIME_MANAGER_HPP
#define ENGINE_TIME_TIME_MANAGER_HPP

#include "../OS/types.h"

namespace engine {

    class TimeManager {
    public:
        ENGINE_INLINE TimeManager() = default;
        ENGINE_INLINE ~TimeManager() = default;

        TimeManager(const TimeManager&)            = delete;
        TimeManager& operator=(const TimeManager&) = delete;
        TimeManager(TimeManager&&)                 = delete;
        TimeManager& operator=(TimeManager&&)      = delete;

        ENGINE_INLINE void Initialize();
        ENGINE_INLINE void Update();

        [[nodiscard]] ENGINE_INLINE f64 GetDeltaTime() const noexcept;
        [[nodiscard]] ENGINE_INLINE f64 GetTotalTime() const noexcept;
        [[nodiscard]] ENGINE_INLINE f64 GetFixedDeltaTime() const noexcept;

        static ENGINE_INLINE void Sleep(u32 milliseconds) noexcept;

    private:
        [[nodiscard]] ENGINE_INLINE u64 GetOSMicroseconds() const noexcept;

        f64 m_deltaTime      = 0.0;
        f64 m_totalTime      = 0.0;
        f64 m_fixedDeltaTime = 1.0 / 60.0;

        u64 m_startupTime  = 0;
        u64 m_previousTime = 0;
        u64 m_frequency    = 0;
    };

} // namespace engine

#include "time_manager.inl"

#endif // ENGINE_TIME_TIME_MANAGER_HPP
