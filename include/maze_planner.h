#ifndef MAZE_PLANNER_H
#define MAZE_PLANNER_H

#include "maze_graph.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAZE_PLANNER_MAX_FRUITS 6

typedef struct {
    int winnable;
    int moves;                          /* moves on the shortest winning route */
    int fruits_collected;
    int path_length;
    int path[MAZE_GRAPH_MAX_CELLS];     /* vertex ids from start to goal */
} MazePlan;

/* Exact search over (position, stamina, fruit taken) with the rules from maze_rules.h.
 * fruit is a list of vertex ids, each collected once; stamina is the amount right now.
 * Returns 1 if the search ran (check plan->winnable), 0 for bad input. */
int MazePlanner_Solve(const MazeGraph *graph, int start, int goal,
                      const int *fruits, int fruit_count, int stamina,
                      MazePlan *plan);

/* Quick check: walks shortest routes between the start, some fruit and the goal in every
 * order. A 1 is always right, a 0 can be wrong (no detours). Cross-checks
 * MazePlanner_Solve in the tests. */
int MazePlanner_QuickWinnable(const MazeGraph *graph, int start, int goal,
                              const int *fruits, int fruit_count, int stamina);

/* Chooses `count` different vertices for fruit, never the start or the goal.
 * The same seed always gives the same fruit. Returns how many were placed. */
int MazePlanner_PlaceFruits(const MazeGraph *graph, int start, int goal,
                            int count, uint32_t seed, int *fruits);

#ifdef __cplusplus
}
#endif

#endif
