#include "maze_planner.h"

#include "maze_rng.h"
#include "maze_rules.h"

#include <stdlib.h>
#include <string.h>

#define STAMINA_LEVELS (MAZE_STAMINA_MAX + 1)
#define UNSEEN (-1)

/* Numbers the fruit 0, 1, 2... in fruit_bit[vertex] (-1 where there is none). Fruit on
 * the start vertex is ignored. Returns how many were numbered, or -1 for bad input. */
static int NumberFruit(const MazeGraph *graph, int start, const int *fruits,
                       int fruit_count, int *fruit_bit, int *fruit_vertex)
{
    int used = 0;

    for (int v = 0; v < graph->vertex_count; v++)
        fruit_bit[v] = -1;

    for (int i = 0; i < fruit_count; i++)
    {
        int v = fruits[i];
        if (v < 0 || v >= graph->vertex_count || fruit_bit[v] >= 0)
            return -1;                      /* not a vertex, or listed twice */
        if (v == start)
            continue;
        fruit_bit[v] = used;
        fruit_vertex[used] = v;
        used++;
    }
    return used;
}

static int InputIsValid(const MazeGraph *graph, int start, int goal,
                        const int *fruits, int fruit_count, int stamina)
{
    return graph && graph->vertex_count > 0 &&
           start >= 0 && start < graph->vertex_count &&
           goal >= 0 && goal < graph->vertex_count &&
           fruit_count >= 0 && fruit_count <= MAZE_PLANNER_MAX_FRUITS &&
           (fruit_count == 0 || fruits) &&
           stamina >= 0 && stamina <= MAZE_STAMINA_MAX;
}

/* A state is (vertex, stamina, fruit taken). BFS over states finds the winning route
 * with the fewest moves. */

int MazePlanner_Solve(const MazeGraph *graph, int start, int goal,
                      const int *fruits, int fruit_count, int stamina,
                      MazePlan *plan)
{
    int fruit_bit[MAZE_GRAPH_MAX_CELLS];
    int fruit_vertex[MAZE_PLANNER_MAX_FRUITS];

    if (!plan || !InputIsValid(graph, start, goal, fruits, fruit_count, stamina))
        return 0;

    int used = NumberFruit(graph, start, fruits, fruit_count, fruit_bit, fruit_vertex);
    if (used < 0)
        return 0;

    memset(plan, 0, sizeof *plan);

    if (start == goal)
    {
        plan->winnable = 1;
        plan->path_length = 1;
        plan->path[0] = start;
        return 1;
    }

    int masks = 1 << used;
    int states = graph->vertex_count * STAMINA_LEVELS * masks;
    int *parent = (int *)malloc((size_t)states * sizeof(int));
    int *queue = (int *)malloc((size_t)states * sizeof(int));
    if (!parent || !queue)
    {
        free(parent);
        free(queue);
        return 0;
    }
    for (int i = 0; i < states; i++)
        parent[i] = UNSEEN;

    int first = (start * STAMINA_LEVELS + stamina) * masks;
    int head = 0;
    int tail = 0;
    int found = UNSEEN;

    parent[first] = first;
    queue[tail++] = first;

    while (head < tail && found == UNSEEN)
    {
        int state = queue[head++];
        int mask = state % masks;
        int rest = state / masks;
        int left = rest % STAMINA_LEVELS;
        int at = rest / STAMINA_LEVELS;

        if (left == 0)
            continue;

        for (const MazeEdgeNode *edge = graph->adjacency[at]; edge && found == UNSEEN; edge = edge->next)
        {
            int to = edge->to;
            int new_left = MazeRules_Move(left);
            int new_mask = mask;
            int bit = fruit_bit[to];

            if (bit >= 0 && !(mask & (1 << bit)))
            {
                new_mask |= 1 << bit;
                new_left = MazeRules_Collect(new_left);
            }

            int next = (to * STAMINA_LEVELS + new_left) * masks + new_mask;
            if (parent[next] != UNSEEN)
                continue;
            parent[next] = state;

            if (to == goal)
                found = next;               /* reaching the exit wins, even with 0 stamina */
            else
                queue[tail++] = next;
        }
    }

    if (found != UNSEEN)
    {
        int backwards[MAZE_GRAPH_MAX_CELLS];
        int length = 0;

        for (int state = found; ; state = parent[state])
        {
            backwards[length++] = (state / masks) / STAMINA_LEVELS;
            if (state == first)
                break;
        }

        plan->winnable = 1;
        plan->path_length = length;
        plan->moves = length - 1;
        for (int i = 0; i < length; i++)
            plan->path[i] = backwards[length - 1 - i];
        for (int mask = found % masks; mask; mask &= mask - 1)
            plan->fruits_collected++;
    }

    free(parent);
    free(queue);
    return 1;
}

#define QUICK_MAX_POINTS (MAZE_PLANNER_MAX_FRUITS + 2)

typedef struct {
    const MazeGraph *graph;
    int points;                             /* start, the fruit, then the goal */
    int goal_point;
    int goal_vertex;
    int fruit_bit[MAZE_GRAPH_MAX_CELLS];
    int leg_length[QUICK_MAX_POINTS][QUICK_MAX_POINTS];   /* vertices on each leg, 0 if none */
    int *leg_path;                          /* leg_path[(a * points + b) * MAX_CELLS ...] */
} QuickSearch;

/* Walks the saved route from point a to b with the real stamina rules. Returns 0 if
 * stamina ran out, 1 on arriving at b, 2 if the goal was reached on the way. */
static int WalkLeg(const QuickSearch *q, int a, int b, int *stamina, int *mask)
{
    const int *path = q->leg_path + (a * q->points + b) * MAZE_GRAPH_MAX_CELLS;

    for (int i = 1; i < q->leg_length[a][b]; i++)
    {
        int v = path[i];
        int bit = q->fruit_bit[v];

        if (*stamina == 0)
            return 0;
        *stamina = MazeRules_Move(*stamina);

        if (bit >= 0 && !(*mask & (1 << bit)))
        {
            *mask |= 1 << bit;
            *stamina = MazeRules_Collect(*stamina);
        }
        if (v == q->goal_vertex)
            return 2;
    }
    return 1;
}

/* Tries going straight to the goal, or to each unvisited fruit and searching again
 * from there. */
static int QuickSearchFrom(const QuickSearch *q, int at, int stamina, int mask, int visited)
{
    int s = stamina;
    int m = mask;

    if (q->leg_length[at][q->goal_point] > 0 && WalkLeg(q, at, q->goal_point, &s, &m) == 2)
        return 1;

    for (int fruit = 1; fruit < q->goal_point; fruit++)
    {
        if ((visited & (1 << fruit)) || q->leg_length[at][fruit] == 0)
            continue;

        s = stamina;
        m = mask;
        int result = WalkLeg(q, at, fruit, &s, &m);
        if (result == 2)
            return 1;
        if (result == 1 && QuickSearchFrom(q, fruit, s, m, visited | (1 << fruit)))
            return 1;
    }
    return 0;
}

int MazePlanner_QuickWinnable(const MazeGraph *graph, int start, int goal,
                              const int *fruits, int fruit_count, int stamina)
{
    QuickSearch q;
    int fruit_vertex[MAZE_PLANNER_MAX_FRUITS];
    int point_vertex[QUICK_MAX_POINTS];

    if (!InputIsValid(graph, start, goal, fruits, fruit_count, stamina))
        return 0;
    if (start == goal)
        return 1;

    int used = NumberFruit(graph, start, fruits, fruit_count, q.fruit_bit, fruit_vertex);
    if (used < 0)
        return 0;

    q.graph = graph;
    q.points = used + 2;
    q.goal_point = used + 1;
    q.goal_vertex = goal;

    point_vertex[0] = start;
    for (int i = 0; i < used; i++)
        point_vertex[i + 1] = fruit_vertex[i];
    point_vertex[q.goal_point] = goal;

    q.leg_path = (int *)malloc((size_t)q.points * q.points * MAZE_GRAPH_MAX_CELLS * sizeof(int));
    if (!q.leg_path)
        return 0;

    for (int a = 0; a < q.points; a++)
    {
        for (int b = 0; b < q.points; b++)
        {
            int va = point_vertex[a];
            int vb = point_vertex[b];

            q.leg_length[a][b] = 0;
            if (a != b)
                q.leg_length[a][b] = MazeGraph_FindShortestPath(
                    graph, graph->vertex_row[va], graph->vertex_col[va],
                    graph->vertex_row[vb], graph->vertex_col[vb],
                    q.leg_path + (a * q.points + b) * MAZE_GRAPH_MAX_CELLS,
                    MAZE_GRAPH_MAX_CELLS);
        }
    }

    int result = QuickSearchFrom(&q, 0, stamina, 0, 0);
    free(q.leg_path);
    return result;
}

int MazePlanner_PlaceFruits(const MazeGraph *graph, int start, int goal,
                            int count, uint32_t seed, int *fruits)
{
    int candidates[MAZE_GRAPH_MAX_CELLS];
    int total = 0;
    uint32_t rng = seed ^ 0x9e3779b9u;

    if (!graph || !fruits || count <= 0)
        return 0;

    for (int v = 0; v < graph->vertex_count; v++)
    {
        if (v != start && v != goal)
            candidates[total++] = v;
    }
    if (count > total)
        count = total;

    /* Shuffle just far enough to pick `count` different vertices. */
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
