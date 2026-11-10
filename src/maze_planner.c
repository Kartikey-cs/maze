#include "maze_planner.h"

#include "maze_features.h"
#include "maze_rng.h"
#include "maze_rules.h"
#include "maze_structs.h"
#include "model/maze_model.h"

#include <stdlib.h>

#define STAMINA_LEVELS (MAZE_STAMINA_MAX + 1)
#define CANDIDATES 12
#define EASIEST_RATING 800.0
#define HARDEST_RATING 1200.0

int MazePlanner_PlaceFruits(const MazeGraph *graph, int start, int goal,
                            int count, uint32_t seed, int *fruits)
{
    int candidates[MAZE_GRAPH_MAX_CELLS];
    int total = 0;
    uint32_t rng = seed ^ 0x9e3779b9u;

    for (int v = 0; v < graph->vertex_count; v++)
    {
        if (v != start && v != goal)
            candidates[total++] = v;
    }
    if (count > total)
        count = total;

    /* Shuffle just far enough to pick count different vertices. */
    for (int i = 0; i < count; i++)
    {
        int j = i + (int)(MazeRng_Next(&rng) % (uint32_t)(total - i));
        int swap = candidates[i];
        candidates[i] = candidates[j];
        candidates[j] = swap;
        fruits[i] = candidates[i];
    }
    return count;
}

/* A state is (vertex, stamina, fruit taken), packed into one number. BFS over states
 * finds the winning route with the fewest moves. */
int MazePlanner_Solve(const MazeGraph *graph, int start, int goal,
                      const int *fruits, int fruit_count, int stamina, int *path)
{
    int fruit_bit[MAZE_GRAPH_MAX_CELLS];

    if (fruit_count > MAZE_PLANNER_MAX_FRUITS || stamina < 0 || stamina > MAZE_STAMINA_MAX)
        return 0;
    if (start == goal)
    {
        path[0] = start;
        return 1;
    }

    /* Each fruit gets one bit, so "which fruit are taken" is a single number. */
    for (int v = 0; v < graph->vertex_count; v++)
        fruit_bit[v] = -1;
    for (int i = 0; i < fruit_count; i++)
        fruit_bit[fruits[i]] = i;

    int masks = 1 << fruit_count;
    int states = graph->vertex_count * STAMINA_LEVELS * masks;
    int *parent = (int *)malloc((size_t)states * sizeof(int));     /* -1 = not reached yet */
    MazeQueue queue;
    if (!parent || !MazeQueue_Init(&queue, states))                /* each state is queued once */
    {
        free(parent);
        return 0;
    }
    for (int i = 0; i < states; i++)
        parent[i] = -1;

    int first = (start * STAMINA_LEVELS + stamina) * masks;
    int found = -1;
    int state;

    parent[first] = first;
    MazeQueue_Push(&queue, first);

    while (found < 0 && MazeQueue_Pop(&queue, &state))
    {
        int mask = state % masks;
        int left = (state / masks) % STAMINA_LEVELS;
        int at = (state / masks) / STAMINA_LEVELS;

        if (left == 0)
            continue;

        for (const MazeEdgeNode *edge = graph->adjacency[at]; edge && found < 0; edge = edge->next)
        {
            int new_left = MazeRules_Move(left);
            int new_mask = mask;
            int bit = fruit_bit[edge->to];

            if (bit >= 0 && !(mask & (1 << bit)))
            {
                new_mask |= 1 << bit;
                new_left = MazeRules_Collect(new_left);
            }

            int next = (edge->to * STAMINA_LEVELS + new_left) * masks + new_mask;
            if (parent[next] >= 0)
                continue;
            parent[next] = state;

            if (edge->to == goal)
                found = next;               /* reaching the exit wins, even with 0 stamina */
            else
                MazeQueue_Push(&queue, next);
        }
    }

    int length = 0;
    if (found >= 0)
    {
        for (int state = found; ; state = parent[state])
        {
            path[length++] = (state / masks) / STAMINA_LEVELS;
            if (state == first)
                break;
        }
        for (int i = 0; i < length / 2; i++)        /* the route was built from the goal back */
        {
            int swap = path[i];
            path[i] = path[length - 1 - i];
            path[length - 1 - i] = swap;
        }
    }

    free(parent);
    MazeQueue_Free(&queue);
    return length;
}

/* Builds one candidate maze. Returns 1 and fills in choice if the maze can be won. */
static int MakeCandidate(MazeDifficulty difficulty, unsigned int seed, MazeChoice *choice)
{
    MazeDifficultyParams params;
    MazeGraph graph;
    MazeFeatures features;
    int grid[MAZE_GRAPH_MAX_CELLS];
    int fruits[MAZE_PLANNER_MAX_FRUITS];
    int path[MAZE_GRAPH_MAX_CELLS];
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

        /* The same fruit the game places for this seed. */
        int count = MazePlanner_PlaceFruits(&graph, start, goal, params.fruit_count, seed, fruits);

        if (MazePlanner_Solve(&graph, start, goal, fruits, count, MAZE_STAMINA_MAX, path) > 0 &&
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

static int EasiestFirst(const void *a, const void *b)
{
    const MazeChoice *p = (const MazeChoice *)a;
    const MazeChoice *q = (const MazeChoice *)b;

    if (p->predicted_success != q->predicted_success)
        return p->predicted_success > q->predicted_success ? -1 : 1;
    return p->seed < q->seed ? -1 : 1;      /* equal guesses: the lower seed first */
}

int MazeAdaptive_Choose(double rating, unsigned int base_seed, MazeChoice *choice)
{
    MazeChoice candidates[CANDIDATES];
    int count = 0;

    for (int i = 0; i < CANDIDATES; i++)
    {
        unsigned int seed = base_seed + (unsigned int)i * 7919u;
        if (seed == 0)
            seed = 1;                       /* the generator treats 0 as "use the clock" */

        if (MakeCandidate((MazeDifficulty)(i % 3), seed, &candidates[count]))
            count++;
    }
    if (count == 0)
        return 0;

    qsort(candidates, (size_t)count, sizeof candidates[0], EasiestFirst);

    /* A rating of 800 or less gets the easiest maze, 1200 or more the hardest. */
    double target = (rating - EASIEST_RATING) / (HARDEST_RATING - EASIEST_RATING);
    if (target < 0.0)
        target = 0.0;
    if (target > 1.0)
        target = 1.0;

    *choice = candidates[(int)(target * (count - 1) + 0.5)];
    return 1;
}
