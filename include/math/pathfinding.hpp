#ifndef ENGINE_MATH_PATHFINDING_HPP
#define ENGINE_MATH_PATHFINDING_HPP

#include "algebra.hpp"
#include <vector>
#include <queue>
#include <unordered_map>
#include <functional>

namespace engine {
namespace math {

    using NodeID = u32;

    class IGraph {
    public:
        virtual ~IGraph() = default;
        virtual f32 Heuristic(NodeID a, NodeID b) const = 0;
        virtual f32 Cost(NodeID a, NodeID b) const = 0;
        virtual std::vector<NodeID> GetNeighbors(NodeID node) const = 0;
    };

    class AStar {
    public:
        static ENGINE_INLINE bool FindPath(const IGraph& graph, NodeID start, NodeID goal, std::vector<NodeID>& out_path);
    };

} // namespace math
} // namespace engine

#include "pathfinding.inl"

#endif // ENGINE_MATH_PATHFINDING_HPP
