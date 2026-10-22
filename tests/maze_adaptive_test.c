#include "maze_adaptive.h"
#include "maze_graph.h"
#include "maze_planner.h"
#include "maze_rules.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void TestTarget(void)
{
    assert(MazeAdaptive_Target(0) == 0.0);
    assert(MazeAdaptive_Target(800) == 0.0);
    assert(MazeAdaptive_Target(1000) == 0.5);
    assert(MazeAdaptive_Target(1200) == 1.0);
    assert(MazeAdaptive_Target(5000) == 1.0);
    for (double r = 700; r < 1300; r += 25)
        assert(MazeAdaptive_Target(r) <= MazeAdaptive_Target(r + 25));
}

/* Rebuilds the chosen maze the way the game does and checks it can be won. */
static void CheckChoiceIsWinnable(const MazeChoice *choice)
{
    MazeDifficultyParams params;
    MazeGraph graph;
    MazePlan plan;
    int grid[MAZE_GRAPH_MAX_CELLS];
    int fruits[MAZE_PLANNER_MAX_FRUITS];

    MazeGen_GetParams(choice->difficulty, &params);
    assert(MazeGen_Generate(grid, params.rows, params.cols, params.loop_percent, choice->seed));
    MazeGraph_Init(&graph, params.rows, params.cols);
    assert(MazeGraph_BuildFromGrid(&graph, grid, params.rows, params.cols));

    int start = MazeGraph_VertexAt(&graph, 1, 1);
    int goal = MazeGraph_VertexAt(&graph, params.rows - 2, params.cols - 2);
    int count = MazePlanner_PlaceFruits(&graph, start, goal, params.fruit_count, choice->seed, fruits);

    assert(MazePlanner_Solve(&graph, start, goal, fruits, count, MAZE_STAMINA_MAX, &plan));
    assert(plan.winnable);
    MazeGraph_Free(&graph);
}

int main(void)
{
    TestTarget();

    assert(!MazeAdaptive_Choose(1000, 1, NULL));

    int hard_for_strong = 0;

    for (unsigned int base = 1; base <= 25; base++)
    {
        MazeChoice weak, middle, strong, again;

        assert(MazeAdaptive_Choose(600, base * 1000, &weak));
        assert(MazeAdaptive_Choose(1000, base * 1000, &middle));
        assert(MazeAdaptive_Choose(1400, base * 1000, &strong));
        assert(MazeAdaptive_Choose(1000, base * 1000, &again));

        /* The same input gives the same maze. */
        assert(middle.difficulty == again.difficulty && middle.seed == again.seed);

        /* A stronger player never gets an easier maze than a weaker one. */
        assert(weak.predicted_success >= middle.predicted_success);
        assert(middle.predicted_success >= strong.predicted_success);

        /* Whatever is chosen can really be won. */
        CheckChoiceIsWinnable(&weak);
        CheckChoiceIsWinnable(&middle);
        CheckChoiceIsWinnable(&strong);

        if (strong.difficulty == MAZE_DIFFICULTY_HARD)
            hard_for_strong++;
    }

    /* The strongest players should often be given the Hard preset. */
    assert(hard_for_strong > 0);
    printf("  a rating of 1400 was given a Hard maze in %d of 25 picks\n", hard_for_strong);

    puts("Adaptive tests PASS");
    return 0;
}
