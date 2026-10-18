// Solver comparison: runs BFS, DFS, Dijkstra and A* on the same generated mazes
// and prints how many squares each one looked at, how long its route was, and how
// long it took.
//
//   ./build/maze_lab [number_of_mazes_per_difficulty]

#include "maze_generator.h"
#include "maze_graph.h"
#include "maze_solvers.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

constexpr int timingRuns = 20;      // each solver is timed this many times per maze

}

int main(int argc, char** argv)
{
    int mazeCount = 50;
    if (argc > 1)
        mazeCount = std::atoi(argv[1]);
    if (mazeCount < 1 || mazeCount > 1000)
    {
        std::fprintf(stderr, "Usage: %s [mazes per difficulty, 1 to 1000]\n", argv[0]);
        return 1;
    }

    BfsSolver bfs;
    DfsSolver dfs;
    DijkstraSolver dijkstra;
    AStarSolver astar;
    const Solver* solvers[] = {&bfs, &dfs, &dijkstra, &astar};
    const int solverCount = 4;

    const char* names[] = {"Easy", "Medium", "Hard"};

    for (int difficulty = 0; difficulty < 3; difficulty++)
    {
        MazeDifficultyParams params;
        MazeGen_GetParams(static_cast<MazeDifficulty>(difficulty), &params);

        double visited[4] = {0};
        double length[4] = {0};
        double micros[4] = {0};

        for (int seed = 1; seed <= mazeCount; seed++)
        {
            int grid[MAZE_GRAPH_MAX_CELLS];
            MazeGraph graph;

            MazeGen_Generate(grid, params.rows, params.cols, params.loop_percent,
                             static_cast<unsigned int>(seed));
            MazeGraph_Init(&graph, params.rows, params.cols);
            MazeGraph_BuildFromGrid(&graph, grid, params.rows, params.cols);

            int start = MazeGraph_VertexAt(&graph, 1, 1);
            int goal = MazeGraph_VertexAt(&graph, params.rows - 2, params.cols - 2);

            for (int s = 0; s < solverCount; s++)
            {
                SolveResult result = solvers[s]->Solve(graph, start, goal);
                visited[s] += result.visited;
                length[s] += result.pathLength;

                double total = 0;
                for (int run = 0; run < timingRuns; run++)
                    total += solvers[s]->Solve(graph, start, goal).microseconds;
                micros[s] += total / timingRuns;
            }

            MazeGraph_Free(&graph);
        }

        std::printf("\n%s (%d x %d), average over %d mazes\n", names[difficulty],
                    params.rows, params.cols, mazeCount);
        std::printf("  %-10s %16s %14s %12s\n", "Solver", "squares checked", "route length", "time (us)");
        for (int s = 0; s < solverCount; s++)
        {
            std::printf("  %-10s %16.1f %14.1f %12.1f\n", solvers[s]->Name(),
                        visited[s] / mazeCount, length[s] / mazeCount, micros[s] / mazeCount);
        }
    }
    return 0;
}
