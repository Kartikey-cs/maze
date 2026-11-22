/* maze_planner.h - stamina rules, fruit and the route planner (maze_planner.c) */
#ifndef MAZE_PLANNER_H
#define MAZE_PLANNER_H

#include "maze_graph.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STAMINA_MAX 100
#define FRUIT_STAMINA 20
#define MAX_FRUITS 6

int stamina_after_move(int stamina);
int stamina_after_fruit(int stamina);

/* picks count different cells for fruit (never start or goal).
   the same seed always gives the same fruit. returns how many were placed */
int place_fruits(const Graph *g, int start, int goal, int count, uint32_t seed, int *fruits);

/* shortest route that wins with the stamina rules, starting with this much stamina.
   fruits are vertex numbers (each eaten once). path needs room for MAX_CELLS.
   returns how many cells the route has, or 0 if it can't be won */
int plan_route(const Graph *g, int start, int goal, const int *fruits, int fruit_count,
               int stamina, int *path);

#ifdef __cplusplus
}
#endif

#endif
