#include "maze_solvers.hpp"

#include "maze_structs.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>

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
    MazeQueue todo;
    int v;

    if (!MazeQueue_Init(&todo, graph.vertex_count))       // each square is queued once
        return;

    seen[start] = 1;
    MazeQueue_Push(&todo, start);

    while (MazeQueue_Pop(&todo, &v))
    {
        result.visited++;
        if (v == goal)
            break;

        for (const MazeEdgeNode* edge = graph.adjacency[v]; edge; edge = edge->next)
        {
            if (!seen[edge->to])
            {
                seen[edge->to] = 1;
                cameFrom[edge->to] = v;
                MazeQueue_Push(&todo, edge->to);
            }
        }
    }
    MazeQueue_Free(&todo);

    if (seen[goal])
        BuildPath(cameFrom, start, goal, result);
}

void DfsSolver::Run(const MazeGraph& graph, int start, int goal, SolveResult& result) const
{
    std::vector<int> cameFrom(graph.vertex_count, -1);
    std::vector<char> seen(graph.vertex_count, 0);
    MazeStack todo;
    int v;

    if (!MazeStack_Init(&todo, graph.vertex_count))       // each square is pushed once
        return;

    seen[start] = 1;
    MazeStack_Push(&todo, start);

    while (MazeStack_Pop(&todo, &v))
    {
        result.visited++;
        if (v == goal)
            break;

        for (const MazeEdgeNode* edge = graph.adjacency[v]; edge; edge = edge->next)
        {
            if (!seen[edge->to])
            {
                seen[edge->to] = 1;
                cameFrom[edge->to] = v;
                MazeStack_Push(&todo, edge->to);
            }
        }
    }
    MazeStack_Free(&todo);

    if (seen[goal])
        BuildPath(cameFrom, start, goal, result);
}

void DijkstraSolver::Run(const MazeGraph& graph, int start, int goal, SolveResult& result) const
{
    const int unreached = graph.vertex_count + 1;     // longer than any real route
    std::vector<int> cameFrom(graph.vertex_count, -1);
    std::vector<int> distance(graph.vertex_count, unreached);
    std::vector<char> done(graph.vertex_count, 0);
    MazeHeap heap;
    int v;

    // A square is pushed when a move to it gets shorter, at most once per move (4 per square).
    if (!MazeHeap_Init(&heap, 4 * graph.vertex_count + 1))
        return;

    distance[start] = 0;
    MazeHeap_Push(&heap, Guess(graph, start, goal), start);

    while (MazeHeap_Pop(&heap, &v))
    {
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
                MazeHeap_Push(&heap, newDistance + Guess(graph, edge->to, goal), edge->to);
            }
        }
    }
    MazeHeap_Free(&heap);

    if (distance[goal] != unreached)
        BuildPath(cameFrom, start, goal, result);
}

// Rows plus columns to the goal. Never too big, so A* still finds a shortest route.
int AStarSolver::Guess(const MazeGraph& graph, int v, int goal) const
{
    return std::abs(graph.vertex_row[v] - graph.vertex_row[goal]) +
           std::abs(graph.vertex_col[v] - graph.vertex_col[goal]);
}
