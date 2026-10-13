#ifndef MAZE_SIM_H
#define MAZE_SIM_H
#include "maze_graph.h"
#include "maze_rules.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { MAZE_BOT_RANDOM, MAZE_BOT_GREEDY } MazeBot;
/* Random chooses a legal neighbor uniformly. Greedy minimizes visits, then
 * Manhattan distance to the goal; ties are random. Neither uses BFS.
 * Fruits are vertex IDs and collected once per trial. Returns -1 for invalid
 * input, 0 for exhaustion/stuck, 1 for success (including the last stamina move).
 * graph must come from MazeGraph_BuildFromGrid. */
int MazeSim_Run(const MazeGraph *graph, int start, int goal, const int *fruits,
                int fruit_count, MazeBot bot, uint32_t seed);
#ifdef __cplusplus
}
#endif
#endif
