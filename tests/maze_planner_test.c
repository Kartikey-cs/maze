#include "maze_generator.h"
#include "maze_graph.h"
#include "maze_planner.h"
#include "maze_rules.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int HasEdge(const MazeGraph *graph, int from, int to)
{
    for (const MazeEdgeNode *edge = graph->adjacency[from]; edge; edge = edge->next)
    {
        if (edge->to == to)
            return 1;
    }
    return 0;
}

/* Plays the plan's path with the real rules: real edges only, never a move with 0 stamina. */
static void CheckPlanIsPlayable(const MazeGraph *graph, const MazePlan *plan, int start, int goal,
                                const int *fruits, int fruit_count, int stamina)
{
    int taken[MAZE_PLANNER_MAX_FRUITS] = {0};

    assert(plan->winnable);
    assert(plan->path_length == plan->moves + 1);
    assert(plan->path[0] == start);
    assert(plan->path[plan->path_length - 1] == goal);

    for (int i = 1; i < plan->path_length; i++)
    {
        int at = plan->path[i];

        assert(HasEdge(graph, plan->path[i - 1], at));
        assert(stamina > 0);
        stamina = MazeRules_Move(stamina);

        for (int f = 0; f < fruit_count; f++)
        {
            if (fruits[f] == at && !taken[f] && at != start)
            {
                taken[f] = 1;
                stamina = MazeRules_Collect(stamina);
            }
        }
    }
}

/* A snake: four rows joined at alternating ends, 126 moves from (0,0) to (6,0). */
static void BuildSnake(MazeGraph *graph)
{
    int grid[7 * 31];

    for (int i = 0; i < 7 * 31; i++)
        grid[i] = 1;
    for (int row = 0; row < 7; row += 2)
        for (int col = 0; col < 31; col++)
            grid[row * 31 + col] = 0;
    grid[1 * 31 + 30] = 0;
    grid[3 * 31 + 0] = 0;
    grid[5 * 31 + 30] = 0;

    MazeGraph_Init(graph, 7, 31);
    assert(MazeGraph_BuildFromGrid(graph, grid, 7, 31));
}

static void TestSnake(void)
{
    MazeGraph graph;
    MazePlan plan;
    BuildSnake(&graph);

    int start = MazeGraph_VertexAt(&graph, 0, 0);
    int at100 = MazeGraph_VertexAt(&graph, 6, 26);
    int at101 = MazeGraph_VertexAt(&graph, 6, 25);
    int end = MazeGraph_VertexAt(&graph, 6, 0);

    /* 100 moves is exactly enough, 101 is not. */
    assert(MazePlanner_Solve(&graph, start, at100, NULL, 0, 100, &plan));
    assert(plan.winnable && plan.moves == 100);
    CheckPlanIsPlayable(&graph, &plan, start, at100, NULL, 0, 100);

    assert(MazePlanner_Solve(&graph, start, at101, NULL, 0, 100, &plan));
    assert(!plan.winnable);

    /* A fruit on the 100th square pays for the one extra move. */
    assert(MazePlanner_Solve(&graph, start, at101, &at100, 1, 100, &plan));
    assert(plan.winnable && plan.moves == 101 && plan.fruits_collected == 1);
    CheckPlanIsPlayable(&graph, &plan, start, at101, &at100, 1, 100);

    /* 126 moves needs two fruit; one is not enough. */
    int two[2] = {MazeGraph_VertexAt(&graph, 4, 10), MazeGraph_VertexAt(&graph, 6, 20)};
    assert(MazePlanner_Solve(&graph, start, end, NULL, 0, 100, &plan) && !plan.winnable);
    assert(MazePlanner_Solve(&graph, start, end, two, 1, 100, &plan) && !plan.winnable);
    assert(MazePlanner_Solve(&graph, start, end, two, 2, 100, &plan));
    assert(plan.winnable && plan.moves == 126 && plan.fruits_collected == 2);
    CheckPlanIsPlayable(&graph, &plan, start, end, two, 2, 100);

    /* The player may be part way through the game, with less stamina. */
    int at50 = MazeGraph_VertexAt(&graph, 2, 12);
    int at51 = MazeGraph_VertexAt(&graph, 2, 11);
    assert(MazePlanner_Solve(&graph, start, at50, NULL, 0, 50, &plan) && plan.winnable);
    assert(MazePlanner_Solve(&graph, start, at51, NULL, 0, 50, &plan) && !plan.winnable);

    /* Standing on the goal already. */
    assert(MazePlanner_Solve(&graph, start, start, NULL, 0, 0, &plan));
    assert(plan.winnable && plan.moves == 0 && plan.path_length == 1);

    /* The quick check agrees on these simple cases. */
    assert(MazePlanner_QuickWinnable(&graph, start, at100, NULL, 0, 100));
    assert(!MazePlanner_QuickWinnable(&graph, start, at101, NULL, 0, 100));
    assert(MazePlanner_QuickWinnable(&graph, start, at101, &at100, 1, 100));
    assert(!MazePlanner_QuickWinnable(&graph, start, end, two, 1, 100));
    assert(MazePlanner_QuickWinnable(&graph, start, end, two, 2, 100));

    MazeGraph_Free(&graph);
}

static void TestBadInput(void)
{
    MazeGraph graph;
    MazePlan plan;
    int fruits[MAZE_PLANNER_MAX_FRUITS + 1] = {1, 2, 3, 4, 5, 6, 7};

    BuildSnake(&graph);

    assert(!MazePlanner_Solve(&graph, 0, 5, NULL, 0, 100, NULL));
    assert(!MazePlanner_Solve(NULL, 0, 5, NULL, 0, 100, &plan));
    assert(!MazePlanner_Solve(&graph, -1, 5, NULL, 0, 100, &plan));
    assert(!MazePlanner_Solve(&graph, 0, graph.vertex_count, NULL, 0, 100, &plan));
    assert(!MazePlanner_Solve(&graph, 0, 5, NULL, 1, 100, &plan));            /* count without a list */
    assert(!MazePlanner_Solve(&graph, 0, 5, fruits, MAZE_PLANNER_MAX_FRUITS + 1, 100, &plan));
    assert(!MazePlanner_Solve(&graph, 0, 5, fruits, 1, 101, &plan));          /* more than the maximum */
    assert(!MazePlanner_Solve(&graph, 0, 5, fruits, 1, -1, &plan));

    int twice[2] = {3, 3};
    assert(!MazePlanner_Solve(&graph, 0, 5, twice, 2, 100, &plan));
    int outside[1] = {9999};
    assert(!MazePlanner_Solve(&graph, 0, 5, outside, 1, 100, &plan));

    MazeGraph_Free(&graph);
}

static void TestPlaceFruits(void)
{
    MazeGraph graph;
    int a[5], b[5], c[5];

    BuildSnake(&graph);
    int start = MazeGraph_VertexAt(&graph, 0, 0);
    int goal = MazeGraph_VertexAt(&graph, 6, 0);

    assert(MazePlanner_PlaceFruits(&graph, start, goal, 5, 42, a) == 5);
    assert(MazePlanner_PlaceFruits(&graph, start, goal, 5, 42, b) == 5);
    assert(MazePlanner_PlaceFruits(&graph, start, goal, 5, 43, c) == 5);
    assert(memcmp(a, b, sizeof a) == 0);
    assert(memcmp(a, c, sizeof a) != 0);

    for (int i = 0; i < 5; i++)
    {
        assert(a[i] >= 0 && a[i] < graph.vertex_count);
        assert(a[i] != start && a[i] != goal);
        for (int j = 0; j < i; j++)
            assert(a[i] != a[j]);
    }

    assert(MazePlanner_PlaceFruits(&graph, start, goal, 0, 42, a) == 0);
    MazeGraph_Free(&graph);
}

/* Generated mazes: the exact planner must give playable routes, never claim less than
 * the quick check, and never beat plain BFS. */
static void TestGeneratedMazes(void)
{
    const char *names[3] = {"Easy", "Medium", "Hard"};
    int path[MAZE_GRAPH_MAX_CELLS];

    for (int difficulty = 0; difficulty < 3; difficulty++)
    {
        MazeDifficultyParams params;
        int winnable = 0;
        int quick_found = 0;
        const int total = 60;

        MazeGen_GetParams((MazeDifficulty)difficulty, &params);

        for (int seed = 1; seed <= total; seed++)
        {
            int grid[MAZE_GRAPH_MAX_CELLS];
            int fruits[MAZE_PLANNER_MAX_FRUITS];
            MazeGraph graph;
            MazePlan plan;

            assert(MazeGen_Generate(grid, params.rows, params.cols, params.loop_percent, (unsigned int)seed));
            MazeGraph_Init(&graph, params.rows, params.cols);
            assert(MazeGraph_BuildFromGrid(&graph, grid, params.rows, params.cols));

            int start = MazeGraph_VertexAt(&graph, 1, 1);
            int goal = MazeGraph_VertexAt(&graph, params.rows - 2, params.cols - 2);
            int count = MazePlanner_PlaceFruits(&graph, start, goal, params.fruit_count, (uint32_t)seed, fruits);
            assert(count == params.fruit_count);

            assert(MazePlanner_Solve(&graph, start, goal, fruits, count, MAZE_STAMINA_MAX, &plan));
            int quick = MazePlanner_QuickWinnable(&graph, start, goal, fruits, count, MAZE_STAMINA_MAX);

            assert(!quick || plan.winnable);                /* quick yes always means exact yes */

            if (plan.winnable)
            {
                int shortest = MazeGraph_FindShortestPath(&graph, 1, 1, params.rows - 2, params.cols - 2,
                                                          path, MAZE_GRAPH_MAX_CELLS) - 1;
                assert(plan.moves >= shortest);
                CheckPlanIsPlayable(&graph, &plan, start, goal, fruits, count, MAZE_STAMINA_MAX);
                winnable++;
            }
            quick_found += quick;
            MazeGraph_Free(&graph);
        }

        printf("  %-6s %2d of %d mazes can be won (quick check found %d of them)\n",
               names[difficulty], winnable, total, quick_found);
    }
}

int main(void)
{
    TestSnake();
    TestBadInput();
    TestPlaceFruits();
    TestGeneratedMazes();
    puts("Planner tests PASS");
    return 0;
}
