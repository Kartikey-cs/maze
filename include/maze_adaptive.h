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

/* Where a rating falls between the easiest maze (0) and the hardest (1).
 * Ratings of 800 or less give 0, ratings of 1200 or more give 1. These are the
 * same numbers maze_stats.c uses for the Easy and Hard presets. */
double MazeAdaptive_Target(double rating);

/* Picks the next maze for a player with this rating:
 *   1. make MAZE_ADAPTIVE_CANDIDATES mazes from the three presets,
 *   2. drop the ones that cannot be won with the stamina and fruit they have,
 *   3. score the rest with the difficulty model and sort easiest first,
 *   4. take the one that sits at the player's place on that list.
 * The choice is the same for the same rating and base_seed. Generating a maze
 * with choice->difficulty and choice->seed gives the maze that was scored.
 * Returns 1 on success, or 0 if no winnable maze was found. */
int MazeAdaptive_Choose(double rating, unsigned int base_seed, MazeChoice *choice);

#ifdef __cplusplus
}
#endif

#endif
