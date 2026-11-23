#include "maze_generator.h"
#include "maze_graph.h"

#include <stdio.h>
#include <stdlib.h>

#define TEST_ITERATIONS 100

static int test_difficulty(MazeDifficulty diff, const char *label)
{
    MazeDifficultyParams params;
    MazeGen_GetParams(diff, &params);

    int grid[MAZE_GRAPH_MAX_CELLS];
    int path[MAZE_GRAPH_MAX_CELLS];
    int failures = 0;

    for (int i = 0; i < TEST_ITERATIONS; i++)
    {
        unsigned int seed = (unsigned int)(i + 1) * 7919u;

        if (!MazeGen_Generate(grid, params.rows, params.cols, params.loop_percent, seed))
        {
            printf("  [FAIL] %s seed=%u: MazeGen_Generate returned 0\n", label, seed);
            failures++;
            continue;
        }

        MazeGraph graph;
        MazeGraph_Init(&graph, params.rows, params.cols);
        if (!MazeGraph_BuildFromGrid(&graph, grid, params.rows, params.cols))
        {
            printf("  [FAIL] %s seed=%u: BuildFromGrid failed\n", label, seed);
            MazeGraph_Free(&graph);
            failures++;
            continue;
        }

        int goal_row = params.rows - 2;
        int goal_col = params.cols - 2;
        int pathLen = MazeGraph_FindShortestPath(&graph, 1, 1, goal_row, goal_col,
                                                  path, MAZE_GRAPH_MAX_CELLS);
        if (pathLen <= 0)
        {
            printf("  [FAIL] %s seed=%u: no path from (1,1) to (%d,%d)\n",
                   label, seed, goal_row, goal_col);
            failures++;
        }

        int reachable = MazeGraph_DfsReachableCount(&graph, 1, 1);
        if (reachable <= 0)
        {
            printf("  [FAIL] %s seed=%u: DFS reachable count = %d\n", label, seed, reachable);
            failures++;
        }

        MazeGraph_Free(&graph);
    }

    return failures;
}

int main(void)
{
    int total_failures = 0;

    printf("Testing EASY (%d mazes)...\n", TEST_ITERATIONS);
    total_failures += test_difficulty(MAZE_DIFFICULTY_EASY, "EASY");

    printf("Testing MEDIUM (%d mazes)...\n", TEST_ITERATIONS);
    total_failures += test_difficulty(MAZE_DIFFICULTY_MEDIUM, "MEDIUM");

    printf("Testing HARD (%d mazes)...\n", TEST_ITERATIONS);
    total_failures += test_difficulty(MAZE_DIFFICULTY_HARD, "HARD");

    if (total_failures == 0)
        printf("\nAll %d mazes across all difficulties are solvable. PASS\n",
               TEST_ITERATIONS * 3);
    else
        printf("\n%d failures detected. FAIL\n", total_failures);

    return total_failures > 0 ? 1 : 0;
}
