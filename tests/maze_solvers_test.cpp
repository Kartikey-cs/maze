#include "maze_generator.h"
#include "maze_graph.h"
#include "maze_heap.h"
#include "maze_solvers.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>

static bool HasEdge(const MazeGraph& graph, int from, int to)
{
    for (const MazeEdgeNode* edge = graph.adjacency[from]; edge; edge = edge->next)
    {
        if (edge->to == to)
            return true;
    }
    return false;
}

// A route must start and end in the right places and only use real edges.
static void CheckRoute(const MazeGraph& graph, const SolveResult& result, int start, int goal)
{
    assert(result.found);
    assert(result.path.front() == start);
    assert(result.path.back() == goal);
    assert(result.pathLength == static_cast<int>(result.path.size()) - 1);
    for (size_t i = 1; i < result.path.size(); i++)
        assert(HasEdge(graph, result.path[i - 1], result.path[i]));
}

static void TestHeap()
{
    MazeHeap heap;
    MazeHeapItem item;

    MazeHeap_Init(&heap);
    assert(!MazeHeap_Pop(&heap, &item));                    // empty

    // Pushed in a scrambled order, popped smallest first.
    const int priorities[] = {9, 3, 7, 1, 8, 2, 6, 4, 5, 0, 3, 3};
    const int count = 12;
    for (int i = 0; i < count; i++)
        assert(MazeHeap_Push(&heap, priorities[i], i));

    std::vector<int> sorted(priorities, priorities + count);
    std::sort(sorted.begin(), sorted.end());
    for (int i = 0; i < count; i++)
    {
        assert(MazeHeap_Pop(&heap, &item));
        assert(item.priority == sorted[i]);
        assert(priorities[item.vertex] == item.priority);   // the vertex stayed with its priority
    }
    assert(!MazeHeap_Pop(&heap, &item));

    // Mixing pushes and pops still gives the smallest first.
    MazeHeap_Push(&heap, 10, 0);
    MazeHeap_Push(&heap, 5, 1);
    assert(MazeHeap_Pop(&heap, &item) && item.priority == 5);
    MazeHeap_Push(&heap, 1, 2);
    assert(MazeHeap_Pop(&heap, &item) && item.priority == 1);
    assert(MazeHeap_Pop(&heap, &item) && item.priority == 10);

    // A full heap refuses more.
    for (int i = 0; i < MAZE_HEAP_MAX; i++)
        assert(MazeHeap_Push(&heap, i % 50, 0));
    assert(!MazeHeap_Push(&heap, 1, 0));
}

static void TestSmallCases()
{
    BfsSolver bfs;
    DfsSolver dfs;
    DijkstraSolver dijkstra;
    AStarSolver astar;
    const Solver* solvers[] = {&bfs, &dfs, &dijkstra, &astar};

    // A corridor split in two: no route.
    const int split[] = {0, 1, 0};
    MazeGraph graph;
    MazeGraph_Init(&graph, 1, 3);
    assert(MazeGraph_BuildFromGrid(&graph, split, 1, 3));
    for (const Solver* solver : solvers)
    {
        SolveResult result = solver->Solve(graph, MazeGraph_VertexAt(&graph, 0, 0), MazeGraph_VertexAt(&graph, 0, 2));
        assert(!result.found);
        assert(result.path.empty());
    }

    // Start and goal the same: a route of length 0.
    const int open[] = {0, 0, 0};
    assert(MazeGraph_BuildFromGrid(&graph, open, 1, 3));
    for (const Solver* solver : solvers)
    {
        int here = MazeGraph_VertexAt(&graph, 0, 1);
        SolveResult result = solver->Solve(graph, here, here);
        assert(result.found && result.pathLength == 0 && result.path.size() == 1);
    }

    // Bad vertex numbers are refused.
    for (const Solver* solver : solvers)
    {
        assert(!solver->Solve(graph, -1, 0).found);
        assert(!solver->Solve(graph, 0, graph.vertex_count).found);
    }
    MazeGraph_Free(&graph);
}

static void TestGeneratedMazes()
{
    BfsSolver bfs;
    DfsSolver dfs;
    DijkstraSolver dijkstra;
    AStarSolver astar;
    int path[MAZE_GRAPH_MAX_CELLS];
    long dijkstraVisited = 0;
    long astarVisited = 0;

    for (int difficulty = 0; difficulty < 3; difficulty++)
    {
        MazeDifficultyParams params;
        MazeGen_GetParams(static_cast<MazeDifficulty>(difficulty), &params);

        for (int seed = 1; seed <= 40; seed++)
        {
            int grid[MAZE_GRAPH_MAX_CELLS];
            MazeGraph graph;

            assert(MazeGen_Generate(grid, params.rows, params.cols, params.loop_percent,
                                    static_cast<unsigned int>(seed)));
            MazeGraph_Init(&graph, params.rows, params.cols);
            assert(MazeGraph_BuildFromGrid(&graph, grid, params.rows, params.cols));

            int start = MazeGraph_VertexAt(&graph, 1, 1);
            int goal = MazeGraph_VertexAt(&graph, params.rows - 2, params.cols - 2);
            int shortest = MazeGraph_FindShortestPath(&graph, 1, 1, params.rows - 2, params.cols - 2,
                                                      path, MAZE_GRAPH_MAX_CELLS) - 1;

            SolveResult b = bfs.Solve(graph, start, goal);
            SolveResult d = dfs.Solve(graph, start, goal);
            SolveResult j = dijkstra.Solve(graph, start, goal);
            SolveResult a = astar.Solve(graph, start, goal);

            CheckRoute(graph, b, start, goal);
            CheckRoute(graph, d, start, goal);
            CheckRoute(graph, j, start, goal);
            CheckRoute(graph, a, start, goal);

            // BFS, Dijkstra and A* all find a shortest route; DFS finds some route.
            assert(b.pathLength == shortest);
            assert(j.pathLength == shortest);
            assert(a.pathLength == shortest);
            assert(d.pathLength >= shortest);

            assert(a.visited <= graph.vertex_count && b.visited <= graph.vertex_count);
            dijkstraVisited += j.visited;
            astarVisited += a.visited;

            MazeGraph_Free(&graph);
        }
    }

    // The guess in A* should save work overall.
    assert(astarVisited <= dijkstraVisited);
    std::printf("  squares checked over 120 mazes: Dijkstra %ld, A* %ld\n", dijkstraVisited, astarVisited);
}

int main()
{
    TestHeap();
    TestSmallCases();
    TestGeneratedMazes();
    std::puts("Solver tests PASS");
    return 0;
}
