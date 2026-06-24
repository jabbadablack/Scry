#ifndef ENGINE_MATH_PATHFINDING_INL
#define ENGINE_MATH_PATHFINDING_INL

#include <algorithm>

namespace engine {
namespace math {

    struct AStarElement {
        NodeID node;
        f32 f_score;

        bool operator>(const AStarElement& other) const {
            return f_score > other.f_score;
        }
    };

    ENGINE_INLINE bool AStar::FindPath(const IGraph& graph, NodeID start, NodeID goal, std::vector<NodeID>& out_path) {
        std::priority_queue<AStarElement, std::vector<AStarElement>, std::greater<AStarElement>> open_set;
        std::unordered_map<NodeID, NodeID> came_from;
        std::unordered_map<NodeID, f32> g_score;

        g_score[start] = 0.0f;
        open_set.push({start, graph.Heuristic(start, goal)});

        while (!open_set.empty()) {
            AStarElement current_element = open_set.top();
            open_set.pop();

            NodeID current = current_element.node;

            if (current == goal) {
                out_path.clear();
                NodeID temp = goal;
                out_path.push_back(temp);
                while (came_from.find(temp) != came_from.end()) {
                    temp = came_from[temp];
                    out_path.push_back(temp);
                }
                std::reverse(out_path.begin(), out_path.end());
                return true;
            }

            // Skip if we found a better path to current already
            if (g_score.find(current) != g_score.end() && current_element.f_score > g_score[current] + graph.Heuristic(current, goal)) {
                continue;
            }

            for (NodeID neighbor : graph.GetNeighbors(current)) {
                f32 tentative_gScore = g_score[current] + graph.Cost(current, neighbor);

                if (g_score.find(neighbor) == g_score.end() || tentative_gScore < g_score[neighbor]) {
                    came_from[neighbor] = current;
                    g_score[neighbor] = tentative_gScore;
                    f32 f_score = tentative_gScore + graph.Heuristic(neighbor, goal);
                    open_set.push({neighbor, f_score});
                }
            }
        }

        return false;
    }

} // namespace math
} // namespace engine

#endif // ENGINE_MATH_PATHFINDING_INL
