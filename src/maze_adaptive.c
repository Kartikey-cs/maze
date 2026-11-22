/* maze_adaptive.c - the two bots and the adaptive maze chooser (uses the trained model) */
#include "maze_adaptive.h"
#include "maze_planner.h"
#include "maze_model.h"   /* the trained model, made by tools/train_model.py */
#include <math.h>
#include <stdlib.h>

#define CANDIDATES 12                      /* mazes tried by the adaptive chooser */
#define EASIEST_RATING 800.0
#define HARDEST_RATING 1200.0

/* ================= bots ================= */

static int manhattan(const Graph *g, int a, int b)
{
    return abs(g->vertex_row[a] - g->vertex_row[b]) +
           abs(g->vertex_col[a] - g->vertex_col[b]);
}

/* runs one bot from start to goal.
   returns 1 if it reaches the goal, 0 if stamina runs out or it is stuck,
   -1 if the input is wrong */
int run_bot(const Graph *g, int start, int goal, const int *fruits,
            int fruit_count, int bot, uint32_t seed)
{
    int has_fruit[MAX_CELLS] = {0};
    int visits[MAX_CELLS] = {0};
    int i;

    /* check the input first */
    if (g == NULL)
        return -1;
    if (start < 0 || start >= g->vertex_count)
        return -1;
    if (goal < 0 || goal >= g->vertex_count)
        return -1;
    if (fruit_count < 0 || fruit_count > g->vertex_count)
        return -1;
    if (fruit_count > 0 && fruits == NULL)
        return -1;
    if (bot != BOT_RANDOM && bot != BOT_GREEDY)
        return -1;

    /* mark where the fruits are */
    for (i = 0; i < fruit_count; i++)
    {
        int v = fruits[i];
        if (v < 0 || v >= g->vertex_count)
            return -1;
        if (has_fruit[v] == 1)   /* same fruit given two times */
            return -1;
        has_fruit[v] = 1;
    }

    int current = start;
    int stamina = STAMINA_MAX;
    visits[current] = 1;

    /* keep moving until we reach the goal or stamina is over */
    while (current != goal && stamina > 0)
    {
        int choices[4];
        int count = 0;
        Edge *e;

        if (bot == BOT_RANDOM)
        {
            /* random bot: every neighbour is a choice */
            for (e = g->adjacency[current]; e != NULL; e = e->next)
            {
                choices[count] = e->to;
                count++;
            }
        }
        else
        {
            /* greedy bot step 1: find the least visited neighbour,
               and if there is a tie, the one closest to the goal */
            int best_visits = -1;
            int best_distance = -1;
            for (e = g->adjacency[current]; e != NULL; e = e->next)
            {
                int v = e->to;
                int distance = manhattan(g, v, goal);
                if (best_visits == -1 || visits[v] < best_visits ||
                    (visits[v] == best_visits && distance < best_distance))
                {
                    best_visits = visits[v];
                    best_distance = distance;
                }
            }

            /* greedy bot step 2: all neighbours that are equally best */
            for (e = g->adjacency[current]; e != NULL; e = e->next)
            {
                int v = e->to;
                if (visits[v] == best_visits && manhattan(g, v, goal) == best_distance)
                {
                    choices[count] = v;
                    count++;
                }
            }
        }

        if (count == 0)   /* no way to go */
            return 0;

        /* pick one of the choices randomly */
        int pick = next_random(&seed) % count;
        current = choices[pick];
        visits[current]++;

        /* every move costs stamina */
        stamina = stamina_after_move(stamina);

        /* eat the fruit if there is one here */
        if (has_fruit[current] == 1)
        {
            has_fruit[current] = 0;
            stamina = stamina_after_fruit(stamina);
        }
    }

    if (current == goal)
        return 1;
    return 0;
}

/* ================= adaptive mazes ================= */

/* builds one maze and scores it with the model.
   returns 1 (and fills choice) only if the maze can be won */
static int make_candidate(Difficulty difficulty, unsigned int seed, MazeChoice *choice)
{
    Level level = get_level(difficulty);
    Graph g;
    Features f;
    int grid[MAX_CELLS];
    int fruits[MAX_FRUITS];
    int path[MAX_CELLS];
    double arr[FEATURE_COUNT];
    int ok = 0;

    if (!generate_maze(grid, level.rows, level.cols, level.loop_percent, seed))
        return 0;

    graph_init(&g, level.rows, level.cols);
    if (build_graph(&g, grid, level.rows, level.cols))
    {
        int start = vertex_at(&g, 1, 1);
        int goal = vertex_at(&g, level.rows - 2, level.cols - 2);

        /* same fruit the game places for this seed */
        int count = place_fruits(&g, start, goal, level.fruit_count, seed, fruits);

        if (plan_route(&g, start, goal, fruits, count, STAMINA_MAX, path) > 0 &&
            get_features(&g, 1, 1, level.rows - 2, level.cols - 2, &f))
        {
            features_to_array(&f, arr);
            choice->difficulty = difficulty;
            choice->seed = seed;
            choice->predicted_success = predict_success(arr);
            ok = 1;
        }
    }

    graph_free(&g);
    return ok;
}

/* for qsort: easiest maze (highest predicted success) first */
static int easiest_first(const void *a, const void *b)
{
    const MazeChoice *p = (const MazeChoice *)a;
    const MazeChoice *q = (const MazeChoice *)b;

    if (p->predicted_success > q->predicted_success)
        return -1;
    if (p->predicted_success < q->predicted_success)
        return 1;
    if (p->seed < q->seed)   /* same guess: lower seed first */
        return -1;
    return 1;
}

/* makes 12 mazes (easy, medium, hard, easy, ...), drops the ones that can't be won,
   sorts them easiest first and picks one based on the rating */
int choose_adaptive_maze(double rating, unsigned int base_seed, MazeChoice *choice)
{
    MazeChoice candidates[CANDIDATES];
    int count = 0;
    int i;

    for (i = 0; i < CANDIDATES; i++)
    {
        unsigned int seed = base_seed + (unsigned int)i * 7919u;
        if (seed == 0)
            seed = 1;   /* the generator uses the clock for seed 0 */

        if (make_candidate((Difficulty)(i % 3), seed, &candidates[count]))
            count++;
    }
    if (count == 0)
        return 0;

    qsort(candidates, count, sizeof(MazeChoice), easiest_first);

    /* rating 800 or less -> easiest maze, 1200 or more -> hardest */
    double target = (rating - EASIEST_RATING) / (HARDEST_RATING - EASIEST_RATING);
    if (target < 0.0)
        target = 0.0;
    if (target > 1.0)
        target = 1.0;

    *choice = candidates[(int)(target * (count - 1) + 0.5)];
    return 1;
}
