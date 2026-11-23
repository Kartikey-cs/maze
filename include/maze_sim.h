#ifndef MAZE_SIM_H
#define MAZE_SIM_H
#include "maze_graph.h"
#include "maze_rules.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { MAZE_BOT_RANDOM, MAZE_BOT_GREEDY } MazeBot;
/* Random picks a legal neighbour uniformly. Greedy minimises visits, then Manhattan distance
 * to the goal, ties random. Returns -1 for invalid input, 0 if out of stamina or stuck,
 * 1 on success. The graph must come from MazeGraph_BuildFromGrid. */
int MazeSim_Run(const MazeGraph *graph, int start, int goal, const int *fruits,
                int fruit_count, MazeBot bot, uint32_t seed);
#ifdef __cplusplus
}
#endif
#endif
