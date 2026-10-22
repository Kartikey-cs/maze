#ifndef MAZE_ADAPTIVE_H
#define MAZE_ADAPTIVE_H

#include "maze_generator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* How many mazes are made and compared each time. */
#define MAZE_ADAPTIVE_CANDIDATES 12

typedef struct {
    MazeDifficulty difficulty;      /* which preset (size, loops, fruit) to use */
    unsigned int seed;              /* with the difficulty, this gives the exact maze */
    double predicted_success;       /* the model's guess: higher means easier */
} MazeChoice;

/* Where a rating falls between the easiest maze (0) and the hardest (1): 800 or less
 * gives 0, 1200 or more gives 1. Same numbers as maze_stats.c. */
double MazeAdaptive_Target(double rating);

/* Picks the next maze for a player with this rating: make candidates from the three
 * presets, drop the unwinnable ones, score and sort easiest first, then take the one
 * at the player's place on the list. The same rating and base_seed give the same
 * choice, and its difficulty and seed rebuild the exact maze that was scored.
 * Returns 1 on success, 0 if no winnable maze was found. */
int MazeAdaptive_Choose(double rating, unsigned int base_seed, MazeChoice *choice);

#ifdef __cplusplus
}
#endif

#endif
