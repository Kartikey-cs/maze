#include "maze_adaptive.h"

#include "maze_features.h"
#include "maze_graph.h"
#include "maze_planner.h"
#include "maze_rules.h"
#include "model/maze_model.h"

#include <math.h>

#define EASIEST_RATING 800.0
#define HARDEST_RATING 1200.0

double MazeAdaptive_Target(double rating)
{
    double target = (rating - EASIEST_RATING) / (HARDEST_RATING - EASIEST_RATING);
    if (target < 0.0) return 0.0;
    if (target > 1.0) return 1.0;
    return target;
}

/* Builds one candidate maze and checks it. Returns 1 and fills in `choice` if
 * the maze can be won, otherwise 0. */
static int MakeCandidate(MazeDifficulty difficulty, unsigned int seed, MazeChoice *choice)
{
    MazeDifficultyParams params;
    MazeGraph graph;
    MazeFeatures features;
    MazePlan plan;
    int grid[MAZE_GRAPH_MAX_CELLS];
    int fruits[MAZE_PLANNER_MAX_FRUITS];
    double vector[MAZE_FEATURE_COUNT];
    int ok = 0;

    MazeGen_GetParams(difficulty, &params);
    if (!MazeGen_Generate(grid, params.rows, params.cols, params.loop_percent, seed))
        return 0;

    MazeGraph_Init(&graph, params.rows, params.cols);
    if (MazeGraph_BuildFromGrid(&graph, grid, params.rows, params.cols))
    {
        int start = MazeGraph_VertexAt(&graph, 1, 1);
        int goal = MazeGraph_VertexAt(&graph, params.rows - 2, params.cols - 2);
        int count = params.fruit_count;
        if (count > MAZE_PLANNER_MAX_FRUITS)
            count = MAZE_PLANNER_MAX_FRUITS;

        /* This is the same fruit the game places for this seed. */
        count = MazePlanner_PlaceFruits(&graph, start, goal, count, seed, fruits);

        if (MazePlanner_Solve(&graph, start, goal, fruits, count, MAZE_STAMINA_MAX, &plan) &&
            plan.winnable &&
            MazeFeatures_Extract(&graph, 1, 1, params.rows - 2, params.cols - 2, &features))
        {
            MazeFeatures_ToVector(&features, vector);
            choice->difficulty = difficulty;
            choice->seed = seed;
            choice->predicted_success = MazeModel_PredictSuccess(vector);
            ok = 1;
        }
    }

    MazeGraph_Free(&graph);
    return ok;
}

int MazeAdaptive_Choose(double rating, unsigned int base_seed, MazeChoice *choice)
{
    MazeChoice candidates[MAZE_ADAPTIVE_CANDIDATES];
    int count = 0;

    if (!choice)
        return 0;

    /* 1 and 2: make the candidates and keep the winnable ones. The presets take
     * turns, so every difficulty is represented. */
    for (int i = 0; i < MAZE_ADAPTIVE_CANDIDATES; i++)
    {
        unsigned int seed = base_seed + (unsigned int)i * 7919u;
        if (seed == 0)
            seed = 1;                   /* the generator treats 0 as "use the clock" */

        if (MakeCandidate((MazeDifficulty)(i % 3), seed, &candidates[count]))
            count++;
    }
    if (count == 0)
        return 0;

    /* 3: easiest first (a higher predicted success rate means an easier maze).
     * Insertion sort, because the list is tiny. */
    for (int i = 1; i < count; i++)
    {
        MazeChoice item = candidates[i];
        int j = i - 1;
        while (j >= 0 && candidates[j].predicted_success < item.predicted_success)
        {
            candidates[j + 1] = candidates[j];
            j--;
        }
        candidates[j + 1] = item;
    }

    /* 4: a stronger player gets a place further down the list. */
    int index = (int)floor(MazeAdaptive_Target(rating) * (count - 1) + 0.5);
    *choice = candidates[index];
    return 1;
}
