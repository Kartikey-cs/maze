#ifndef MAZE_PLANNER_H
#define MAZE_PLANNER_H

#include "maze_generator.h"
#include "maze_graph.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAZE_PLANNER_MAX_FRUITS 6

/* Chooses count different vertices for fruit, never the start or the goal.
 * The same seed always gives the same fruit. Returns how many were placed. */
int MazePlanner_PlaceFruits(const MazeGraph *graph, int start, int goal,
                            int count, uint32_t seed, int *fruits);

/* Finds the shortest route that wins with the game's stamina rules. fruits holds
 * vertex ids (each collected once) and stamina is what the player has right now.
 * Writes the route to path (room for MAZE_GRAPH_MAX_CELLS) and returns how many
 * vertices it has, or 0 if the maze cannot be won. */
int MazePlanner_Solve(const MazeGraph *graph, int start, int goal,
                      const int *fruits, int fruit_count, int stamina, int *path);

typedef struct {
    MazeDifficulty difficulty;
    unsigned int seed;              /* with the difficulty, this gives the exact maze */
    double predicted_success;       /* the model's guess: higher means easier */
} MazeChoice;

/* Picks the next maze for a player with this rating: makes candidates from the three
 * presets, drops the ones that cannot be won, sorts the rest easiest first and takes
 * the one at the player's place on the list. Returns 1 on success, 0 if none can be won. */
int MazeAdaptive_Choose(double rating, unsigned int base_seed, MazeChoice *choice);

#ifdef __cplusplus
}
#endif

#endif
