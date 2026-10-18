#include "maze_solvers.hpp"

#include "maze_heap.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <queue>
#include <stack>

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
    MazeHeap heap;
    MazeHeapItem item;

    MazeHeap_Init(&heap);
    distance[start] = 0;
    MazeHeap_Push(&heap, 0, start);

    while (MazeHeap_Pop(&heap, &item))
    {
        int v = item.vertex;
        if (item.priority > distance[v])
            continue;                           // an older, longer entry for v
        result.visited++;
        if (v == goal)
            break;

        for (const MazeEdgeNode* edge = graph.adjacency[v]; edge; edge = edge->next)
        {
            int newDistance = distance[v] + 1;  // every move costs 1
            if (newDistance < distance[edge->to])
            {
                distance[edge->to] = newDistance;
                cameFrom[edge->to] = v;
                MazeHeap_Push(&heap, newDistance, edge->to);
            }
        }
    }

    if (distance[goal] != unreached)
        BuildPath(cameFrom, start, goal, result);
}

void AStarSolver::Run(const MazeGraph& graph, int start, int goal, SolveResult& result) const
{
    const int unreached = graph.vertex_count + 1;
    std::vector<int> cameFrom(graph.vertex_count, -1);
    std::vector<int> distance(graph.vertex_count, unreached);
    std::vector<char> done(graph.vertex_count, 0);
    MazeHeap heap;
    MazeHeapItem item;

    // Guess of the moves left: rows plus columns to the goal. It is never too
    // big, so A* still finds a shortest route.
    auto guess = [&](int v) {
        return std::abs(graph.vertex_row[v] - graph.vertex_row[goal]) +
               std::abs(graph.vertex_col[v] - graph.vertex_col[goal]);
    };

    MazeHeap_Init(&heap);
    distance[start] = 0;
    MazeHeap_Push(&heap, guess(start), start);

    while (MazeHeap_Pop(&heap, &item))
    {
        int v = item.vertex;
        if (done[v])
            continue;
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
                MazeHeap_Push(&heap, newDistance + guess(edge->to), edge->to);
            }
        }
    }

    if (distance[goal] != unreached)
        BuildPath(cameFrom, start, goal, result);
}
