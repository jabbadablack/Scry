#ifndef ENGINE_CORE_DEBUG_LOGGER_INL
#define ENGINE_CORE_DEBUG_LOGGER_INL

#include <iostream>

namespace engine {
namespace debug {

    inline AsyncLogger::AsyncLogger() {
        Init();
    }

    inline AsyncLogger::~AsyncLogger() {
        Shutdown();
    }

    inline AsyncLogger& AsyncLogger::Get() {
        static AsyncLogger instance;
        return instance;
    }

    inline void AsyncLogger::Init() {
        if (!m_running.load()) {
            m_running.store(true);
            m_thread = std::thread(&AsyncLogger::ThreadLoop, this);
        }
    }

    inline void AsyncLogger::Shutdown() {
        if (m_running.load()) {
            m_running.store(false);
            m_cv.notify_all();
            if (m_thread.joinable()) {
                m_thread.join();
            }
        }
    }

    inline void AsyncLogger::Log(LogLevel level, const std::string& message) {
        std::string formatted;
        switch (level) {
            case LogLevel::Info:  formatted = "[INFO] " + message; break;
            case LogLevel::Warn:  formatted = "[WARN] " + message; break;
            case LogLevel::Error: formatted = "[ERROR] " + message; break;
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_buffers[m_writeIndex.load(std::memory_order_relaxed)].push_back(std::move(formatted));
        }
        m_cv.notify_one();
    }

    inline void AsyncLogger::ThreadLoop() {
        while (m_running.load()) {
            std::vector<std::string> localBuffer;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this]() {
                    int idx = m_writeIndex.load(std::memory_order_relaxed);
                    return !m_buffers[idx].empty() || !m_running.load();
                });

                int oldIdx = m_writeIndex.load(std::memory_order_relaxed);
                int newIdx = 1 - oldIdx;
                m_writeIndex.store(newIdx, std::memory_order_relaxed);

                localBuffer = std::move(m_buffers[oldIdx]);
                m_buffers[oldIdx].clear();
            }

            for (const auto& msg : localBuffer) {
                if (msg.rfind("[ERROR]", 0) == 0) {
                    std::cerr << msg << std::endl;
                } else {
                    std::cout << msg << std::endl;
                }
            }
        }

        // Flush remaining messages on shutdown
        for (int i = 0; i < 2; ++i) {
            std::vector<std::string> remaining;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                remaining = std::move(m_buffers[i]);
                m_buffers[i].clear();
            }
            for (const auto& msg : remaining) {
                if (msg.rfind("[ERROR]", 0) == 0) {
                    std::cerr << msg << std::endl;
                } else {
                    std::cout << msg << std::endl;
                }
            }
        }
    }

} // namespace debug
} // namespace engine

#endif // ENGINE_CORE_DEBUG_LOGGER_INL
