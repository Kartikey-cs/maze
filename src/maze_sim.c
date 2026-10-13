#include "maze_sim.h"
#include "maze_rng.h"
#include <limits.h>
#include <stdlib.h>

int MazeSim_Run(const MazeGraph *g, int start, int goal, const int *fruits,
                int fruit_count, MazeBot bot, uint32_t seed)
{
    if (!g || start < 0 || goal < 0 || start >= g->vertex_count ||
        goal >= g->vertex_count || fruit_count < 0 ||
        fruit_count > g->vertex_count || (fruit_count && !fruits) ||
        (bot != MAZE_BOT_RANDOM && bot != MAZE_BOT_GREEDY)) return -1;
    int remaining[MAZE_GRAPH_MAX_CELLS] = {0};
    int visits[MAZE_GRAPH_MAX_CELLS] = {0};
    for (int i = 0; i < fruit_count; i++)
    {
        int v = fruits[i];
        if (v < 0 || v >= g->vertex_count || remaining[v]) return -1;
        remaining[v] = 1;
    }
    int at = start, stamina = MAZE_STAMINA_MAX;
    visits[at]++;
    /* Finite even with refills: each fruit can pay for only 20 extra moves. */
    while (at != goal && stamina > 0)
    {
        int choices[4], count = 0, best_visits = INT_MAX, best_distance = INT_MAX;
        for (const MazeEdgeNode *e = g->adjacency[at]; e; e = e->next)
        {
            int v = e->to;
            int distance = abs(g->vertex_row[v] - g->vertex_row[goal]) +
                           abs(g->vertex_col[v] - g->vertex_col[goal]);
            if (bot == MAZE_BOT_GREEDY)
            {
                if (visits[v] < best_visits ||
                    (visits[v] == best_visits && distance < best_distance))
                {
                    count = 0;
                    best_visits = visits[v];
                    best_distance = distance;
                }
                if (visits[v] != best_visits || distance != best_distance) continue;
            }
            choices[count++] = v;
        }
        if (!count) return 0;
        at = choices[MazeRng_Next(&seed) % (uint32_t)count];
        visits[at]++;
        stamina = MazeRules_Move(stamina);
        if (remaining[at])
        {
            remaining[at] = 0;
            stamina = MazeRules_Collect(stamina);
        }
    }
    return at == goal;
}
