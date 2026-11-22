/* maze_adaptive.h - the two bots and the adaptive maze chooser (maze_adaptive.c) */
#ifndef MAZE_ADAPTIVE_H
#define MAZE_ADAPTIVE_H

#include "maze_graph.h"
#include "maze_generator.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BOT_RANDOM 0
#define BOT_GREEDY 1

/* returns 1 = reached goal, 0 = failed, -1 = wrong input */
int run_bot(const Graph *g, int start, int goal, const int *fruits,
            int fruit_count, int bot, uint32_t seed);

typedef struct {
    Difficulty difficulty;
    unsigned int seed;          /* difficulty + seed gives back the exact maze */
    double predicted_success;   /* the model's guess: higher means easier */
} MazeChoice;

/* picks a winnable maze that fits the player's rating. returns 0 if none can be won */
int choose_adaptive_maze(double rating, unsigned int base_seed, MazeChoice *choice);

#ifdef __cplusplus
}
#endif

#endif
