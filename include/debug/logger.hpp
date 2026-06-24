#ifndef ENGINE_CORE_DEBUG_LOGGER_HPP
#define ENGINE_CORE_DEBUG_LOGGER_HPP

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace engine {
namespace debug {

    enum class LogLevel {
        Info,
        Warn,
        Error
    };

    class AsyncLogger {
    public:
        AsyncLogger();
        ~AsyncLogger();

        // Delete copy and move semantics
        AsyncLogger(const AsyncLogger&) = delete;
        AsyncLogger& operator=(const AsyncLogger&) = delete;
        AsyncLogger(AsyncLogger&&) = delete;
        AsyncLogger& operator=(AsyncLogger&&) = delete;

        static AsyncLogger& Get();

        void Init();
        void Shutdown();
        void Log(LogLevel level, const std::string& message);

    private:
        void ThreadLoop();

        std::vector<std::string> m_buffers[2];
        std::atomic<int> m_writeIndex{0};
        
        std::thread m_thread;
        std::mutex m_mutex;
        std::condition_variable m_cv;
        std::atomic<bool> m_running{false};
    };

} // namespace debug
} // namespace engine

#define ENGINE_LOG_INFO(msg) engine::debug::AsyncLogger::Get().Log(engine::debug::LogLevel::Info, msg)
#define ENGINE_LOG_WARN(msg) engine::debug::AsyncLogger::Get().Log(engine::debug::LogLevel::Warn, msg)
#define ENGINE_LOG_ERROR(msg) engine::debug::AsyncLogger::Get().Log(engine::debug::LogLevel::Error, msg)

#include "logger.inl"

#endif // ENGINE_CORE_DEBUG_LOGGER_HPP
