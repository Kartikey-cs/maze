/* maze_planner.c - the stamina rules, fruit placement and the route planner */
#include "maze_planner.h"
#include "maze_generator.h"
#include "maze_structs.h"
#include <stdlib.h>

/* ================= game rules ================= */

int stamina_after_move(int stamina)
{
    if (stamina > 0)
        return stamina - 1;
    return 0;
}

int stamina_after_fruit(int stamina)
{
    stamina = stamina + FRUIT_STAMINA;
    if (stamina > STAMINA_MAX)
        stamina = STAMINA_MAX;
    return stamina;
}

/* ================= planner ================= */

#define STAMINA_LEVELS (STAMINA_MAX + 1)   /* stamina can be 0 to 100 */

int place_fruits(const Graph *g, int start, int goal, int count, uint32_t seed, int *fruits)
{
    int cells[MAX_CELLS];
    int total = 0;
    int i, v;
    uint32_t rng = seed ^ 0x9e3779b9u;   /* mix the seed a bit */

    for (v = 0; v < g->vertex_count; v++)
    {
        if (v != start && v != goal)
        {
            cells[total] = v;
            total++;
        }
    }
    if (count > total)
        count = total;

    /* shuffle only the first count cells (enough to pick count different ones) */
    for (i = 0; i < count; i++)
    {
        int j = i + next_random(&rng) % (total - i);
        int temp = cells[i];
        cells[i] = cells[j];
        cells[j] = temp;
        fruits[i] = cells[i];
    }
    return count;
}

/*
 * BFS where a "state" is (cell, stamina left, which fruits are eaten).
 * The three numbers are packed into one int:
 *     state = (cell * STAMINA_LEVELS + stamina) * masks + eaten
 * "eaten" uses one bit per fruit. The first time BFS reaches the goal
 * is the winning route with the fewest moves.
 */
int plan_route(const Graph *g, int start, int goal, const int *fruits, int fruit_count,
               int stamina, int *path)
{
    int fruit_bit[MAX_CELLS];   /* which bit belongs to the fruit on this cell, or -1 */
    int i, v;

    if (fruit_count > MAX_FRUITS || stamina < 0 || stamina > STAMINA_MAX)
        return 0;
    if (start == goal)
    {
        path[0] = start;
        return 1;
    }

    for (v = 0; v < g->vertex_count; v++)
        fruit_bit[v] = -1;
    for (i = 0; i < fruit_count; i++)
        fruit_bit[fruits[i]] = i;

    int masks = 1 << fruit_count;
    int states = g->vertex_count * STAMINA_LEVELS * masks;

    /* parent[state] = the state we came from, -1 = not reached yet */
    int *parent = (int *)malloc(states * sizeof(int));
    Queue queue;
    if (parent == NULL || !queue_init(&queue, states))   /* each state is queued once */
    {
        free(parent);
        return 0;
    }
    for (i = 0; i < states; i++)
        parent[i] = -1;

    int first = (start * STAMINA_LEVELS + stamina) * masks;
    int found = -1;
    int state;

    parent[first] = first;
    queue_push(&queue, first);

    while (found == -1 && queue_pop(&queue, &state))
    {
        /* unpack the state */
        int eaten = state % masks;
        int left = (state / masks) % STAMINA_LEVELS;
        int at = (state / masks) / STAMINA_LEVELS;

        if (left == 0)   /* can't move any more */
            continue;

        Edge *e;
        for (e = g->adjacency[at]; e != NULL && found == -1; e = e->next)
        {
            int new_left = stamina_after_move(left);
            int new_eaten = eaten;
            int bit = fruit_bit[e->to];

            /* fruit here that we haven't eaten yet */
            if (bit != -1 && (eaten & (1 << bit)) == 0)
            {
                new_eaten = new_eaten | (1 << bit);
                new_left = stamina_after_fruit(new_left);
            }

            int next = (e->to * STAMINA_LEVELS + new_left) * masks + new_eaten;
            if (parent[next] != -1)   /* already seen */
                continue;
            parent[next] = state;

            if (e->to == goal)
                found = next;   /* reaching the exit wins, even with 0 stamina */
            else
                queue_push(&queue, next);
        }
    }

    int length = 0;
    if (found != -1)
    {
        /* walk back from the goal to the start */
        state = found;
        while (1)
        {
            path[length] = (state / masks) / STAMINA_LEVELS;
            length++;
            if (state == first)
                break;
            state = parent[state];
        }
        /* it was built backwards, so flip it */
        for (i = 0; i < length / 2; i++)
        {
            int temp = path[i];
            path[i] = path[length - 1 - i];
            path[length - 1 - i] = temp;
        }
    }

    free(parent);
    queue_free(&queue);
    return length;
}
