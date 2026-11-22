/* maze_graph.h - the maze as a graph, BFS, DFS and the features for the model (maze_graph.c) */
#ifndef MAZE_GRAPH_H
#define MAZE_GRAPH_H

#include "maze_generator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* one node of the adjacency list */
typedef struct Edge {
    int to;
    struct Edge *next;
} Edge;

typedef struct {
    int rows;
    int cols;
    int vertex_count;
    int cell_to_vertex[MAX_ROWS][MAX_COLS];   /* -1 if the cell is a wall */
    int vertex_row[MAX_CELLS];
    int vertex_col[MAX_CELLS];
    Edge *adjacency[MAX_CELLS];
} Graph;

void graph_init(Graph *g, int rows, int cols);
void graph_free(Graph *g);
int build_graph(Graph *g, const int *grid, int rows, int cols);
int vertex_at(const Graph *g, int row, int col);
int bfs_shortest_path(const Graph *g, int start_row, int start_col,
                      int goal_row, int goal_col, int *path, int max_path);
int dfs_count(const Graph *g, int start_row, int start_col);

/* ---------- features for the model ---------- */

#define FEATURE_COUNT 7

typedef struct {
    int vertices;
    int edges;
    int dead_ends;
    int junctions;
    int loops;
    int shortest_path;
    double detour_ratio;
} Features;

int get_features(const Graph *g, int start_row, int start_col,
                 int goal_row, int goal_col, Features *f);
void features_to_array(const Features *f, double out[FEATURE_COUNT]);

#ifdef __cplusplus
}
#endif

#endif
