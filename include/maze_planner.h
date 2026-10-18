#ifndef MAZE_PLANNER_H
#define MAZE_PLANNER_H

#include "maze_graph.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAZE_PLANNER_MAX_FRUITS 6

typedef struct {
    int winnable;                       /* 1 if the exit can be reached */
    int moves;                          /* moves on the shortest winning route */
    int fruits_collected;               /* fruit picked up along that route */
    int path_length;                    /* number of vertices in path */
    int path[MAZE_GRAPH_MAX_CELLS];     /* vertex ids from start to goal */
} MazePlan;

/* Exact search over (position, stamina, fruit already taken), using the same
 * stamina rules as the game (include/maze_rules.h). Fruit is a list of vertex
 * ids and each one can be collected once. `stamina` is how much the player has
 * right now, so this also works in the middle of a game.
 *
 * Returns 1 when the search ran, even if the maze cannot be won (check
 * plan->winnable). Returns 0 for bad input, such as a vertex that does not
 * exist, a fruit listed twice, or more than MAZE_PLANNER_MAX_FRUITS fruit. */
int MazePlanner_Solve(const MazeGraph *graph, int start, int goal,
                      const int *fruits, int fruit_count, int stamina,
                      MazePlan *plan);

/* A much simpler check: walk shortest routes between the start, some of the
 * fruit, and the goal, trying every order, and play each walk with the real
 * rules. Returns 1 if some order reaches the goal. A 1 is always right (the
 * exact search agrees), but a 0 can be wrong because it does not look for
 * detours. Used to cross-check MazePlanner_Solve in the tests. */
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
