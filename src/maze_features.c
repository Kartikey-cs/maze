#include "maze_features.h"
#include <math.h>
#include <stddef.h>

int MazeFeatures_Extract(const MazeGraph *graph, int sr, int sc, int gr, int gc,
                         MazeFeatures *out)
{
    int path[MAZE_GRAPH_MAX_CELLS];
    if (!out) return 0;
    *out = (MazeFeatures){0};
    if (!graph || graph->vertex_count <= 0 ||
        MazeGraph_DfsReachableCount(graph, sr, sc) != graph->vertex_count)
        return 0;
    int count = MazeGraph_FindShortestPath(graph, sr, sc, gr, gc,
                                          path, MAZE_GRAPH_MAX_CELLS);
    if (!count) return 0;

    out->vertices = graph->vertex_count;
    int degree_sum = 0;
    for (int v = 0; v < graph->vertex_count; v++)
    {
        int degree = 0;
        for (const MazeEdgeNode *e = graph->adjacency[v]; e; e = e->next) degree++;
        degree_sum += degree;
        out->dead_ends += degree == 1;
        out->junctions += degree >= 3;
    }
    out->edges = degree_sum / 2;
    out->loops = out->edges - out->vertices + 1;
    out->shortest_path = count - 1;
    double distance = hypot((double)gr - sr, (double)gc - sc);
    out->detour_ratio = distance > 0 ? out->shortest_path / distance : 1.0;
    return 1;
}

void MazeFeatures_ToVector(const MazeFeatures *f, double out[MAZE_FEATURE_COUNT])
{
    out[0] = f->vertices;
    out[1] = f->edges;
    out[2] = f->dead_ends;
    out[3] = f->junctions;
    out[4] = f->loops;
    out[5] = f->shortest_path;
    out[6] = f->detour_ratio;
}
