#include "maze_solvers.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <queue>
#include <stack>

namespace {

// Min-heap in a vector: the smallest priority is always first, and the parent of item i
// is item (i - 1) / 2.
class MinHeap {
public:
    bool Empty() const { return items_.empty(); }

    void Push(int priority, int vertex)
    {
        items_.push_back({priority, vertex});
        size_t i = items_.size() - 1;
        while (i > 0 && items_[(i - 1) / 2].priority > items_[i].priority)
        {
            std::swap(items_[(i - 1) / 2], items_[i]);
            i = (i - 1) / 2;
        }
    }

    // Removes the item with the smallest priority and returns its vertex.
    int Pop()
    {
        int top = items_[0].vertex;
        items_[0] = items_.back();
        items_.pop_back();

        size_t i = 0;
        while (2 * i + 1 < items_.size())
        {
            size_t child = 2 * i + 1;
            if (child + 1 < items_.size() && items_[child + 1].priority < items_[child].priority)
                child++;
            if (items_[i].priority <= items_[child].priority)
                break;
            std::swap(items_[i], items_[child]);
            i = child;
        }
        return top;
    }

private:
    struct Item {
        int priority;
        int vertex;
    };
    std::vector<Item> items_;
};

} // namespace

SolveResult Solver::Solve(const MazeGraph& graph, int start, int goal) const
{
    SolveResult result;
    if (start < 0 || goal < 0 || start >= graph.vertex_count || goal >= graph.vertex_count)
        return result;

    auto before = std::chrono::steady_clock::now();
    Run(graph, start, goal, result);
    auto after = std::chrono::steady_clock::now();

    result.microseconds = std::chrono::duration<double, std::micro>(after - before).count();
    return result;
}

void Solver::BuildPath(const std::vector<int>& cameFrom, int start, int goal, SolveResult& result)
{
    for (int v = goal; v != -1; v = cameFrom[v])
    {
        result.path.push_back(v);
        if (v == start)
            break;
    }
    std::reverse(result.path.begin(), result.path.end());
    result.found = true;
    result.pathLength = static_cast<int>(result.path.size()) - 1;
}

void BfsSolver::Run(const MazeGraph& graph, int start, int goal, SolveResult& result) const
{
    std::vector<int> cameFrom(graph.vertex_count, -1);
    std::vector<char> seen(graph.vertex_count, 0);
    std::queue<int> todo;

    seen[start] = 1;
    todo.push(start);

    while (!todo.empty())
    {
        int v = todo.front();
        todo.pop();
        result.visited++;
        if (v == goal)
            break;

        for (const MazeEdgeNode* edge = graph.adjacency[v]; edge; edge = edge->next)
        {
            if (!seen[edge->to])
            {
                seen[edge->to] = 1;
                cameFrom[edge->to] = v;
                todo.push(edge->to);
            }
        }
    }

    if (seen[goal])
        BuildPath(cameFrom, start, goal, result);
}

void DfsSolver::Run(const MazeGraph& graph, int start, int goal, SolveResult& result) const
{
    std::vector<int> cameFrom(graph.vertex_count, -1);
    std::vector<char> seen(graph.vertex_count, 0);
    std::stack<int> todo;

    seen[start] = 1;
    todo.push(start);

    while (!todo.empty())
    {
        int v = todo.top();
        todo.pop();
        result.visited++;
        if (v == goal)
            break;

        for (const MazeEdgeNode* edge = graph.adjacency[v]; edge; edge = edge->next)
        {
            if (!seen[edge->to])
            {
                seen[edge->to] = 1;
                cameFrom[edge->to] = v;
                todo.push(edge->to);
            }
        }
    }

    if (seen[goal])
        BuildPath(cameFrom, start, goal, result);
}

void DijkstraSolver::Run(const MazeGraph& graph, int start, int goal, SolveResult& result) const
{
    const int unreached = graph.vertex_count + 1;     // longer than any real route
    std::vector<int> cameFrom(graph.vertex_count, -1);
    std::vector<int> distance(graph.vertex_count, unreached);
    std::vector<char> done(graph.vertex_count, 0);
    MinHeap heap;

    distance[start] = 0;
    heap.Push(Guess(graph, start, goal), start);

    while (!heap.Empty())
    {
        int v = heap.Pop();
        if (done[v])
            continue;                                 // an older, longer entry for v
        done[v] = 1;
        result.visited++;
        if (v == goal)
            break;

        for (const MazeEdgeNode* edge = graph.adjacency[v]; edge; edge = edge->next)
        {
            int newDistance = distance[v] + 1;
            if (newDistance < distance[edge->to])
            {
                distance[edge->to] = newDistance;
                cameFrom[edge->to] = v;
                heap.Push(newDistance + Guess(graph, edge->to, goal), edge->to);
            }
        }
    }

    if (distance[goal] != unreached)
        BuildPath(cameFrom, start, goal, result);
}

// Rows plus columns to the goal. Never too big, so A* still finds a shortest route.
int AStarSolver::Guess(const MazeGraph& graph, int v, int goal) const
{
    return std::abs(graph.vertex_row[v] - graph.vertex_row[goal]) +
           std::abs(graph.vertex_col[v] - graph.vertex_col[goal]);
}
