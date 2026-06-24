#ifndef ENGINE_MODULE_SYSTEM_BUILDER_HPP
#define ENGINE_MODULE_SYSTEM_BUILDER_HPP

#include "module.hpp"
#include "../intent/intent_queue.hpp"
#include <utility>

namespace engine {

    class SystemBuilder {
    public:
        ENGINE_INLINE explicit SystemBuilder(FrameDAG& dag) : m_dag(dag) {}

        template <typename F>
        ENGINE_INLINE SystemBuilder& AddIntent(const char* name, F&& func) {
            m_dag.taskflow.emplace(std::forward<F>(func)).name(name).precede(m_dag.phase_intent);
            return *this;
        }

        template <typename F>
        ENGINE_INLINE SystemBuilder& AddReactor(const char* name, F&& func) {
            auto task = m_dag.taskflow.emplace(std::forward<F>(func)).name(name);
            m_dag.phase_intent.precede(task);
            task.precede(m_dag.phase_reactor);
            return *this;
        }

        template <typename F>
        ENGINE_INLINE SystemBuilder& AddExtractor(const char* name, F&& func) {
            auto task = m_dag.taskflow.emplace(std::forward<F>(func)).name(name);
            m_dag.phase_reactor.precede(task);
            task.precede(m_dag.phase_extract);
            return *this;
        }

    private:
        FrameDAG& m_dag;
    };

    // Helper to abstract queue iteration and state validation away from the user
    template <typename Q, typename F>
    ENGINE_INLINE auto Process(Q& queue, F&& func) {
        return [&queue, func = std::forward<F>(func)]() mutable {
            for (auto* it = queue.begin(); it != queue.end(); ++it) {
                if (it->state == IntentState::Pending && it->data) {
                    func(*(it->data));
                    it->state = IntentState::Consumed;
                }
            }
        };
    }

} // namespace engine

#endif // ENGINE_MODULE_SYSTEM_BUILDER_HPP
