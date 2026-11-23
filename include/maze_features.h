#ifndef MAZE_FEATURES_H
#define MAZE_FEATURES_H
#include "maze_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAZE_FEATURE_COUNT 7
typedef struct MazeFeatures {
    int vertices, edges, dead_ends, junctions, loops, shortest_path;
    double detour_ratio;
} MazeFeatures;

/* Needs a connected graph from MazeGraph_BuildFromGrid. Returns 0 on invalid endpoints or an
 * empty or disconnected graph. Distance is Euclidean in grid cells. */
int MazeFeatures_Extract(const MazeGraph *graph, int start_row, int start_col,
                         int goal_row, int goal_col, MazeFeatures *out);
/* Stable order shared with Python and exported model headers. */
void MazeFeatures_ToVector(const MazeFeatures *features, double out[MAZE_FEATURE_COUNT]);

#ifdef __cplusplus
}
#endif
#endif
