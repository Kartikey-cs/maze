#ifndef MAZE_GRAPH_H
#define MAZE_GRAPH_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAZE_GRAPH_MAX_ROWS 32
#define MAZE_GRAPH_MAX_COLS 32
#define MAZE_GRAPH_MAX_CELLS (MAZE_GRAPH_MAX_ROWS * MAZE_GRAPH_MAX_COLS)

typedef struct MazeEdgeNode {
    int to;
    struct MazeEdgeNode *next;
} MazeEdgeNode;

typedef struct MazeGraph {
    int rows;
    int cols;
    int vertex_count;
    int cell_to_vertex[MAZE_GRAPH_MAX_ROWS][MAZE_GRAPH_MAX_COLS];
    int vertex_row[MAZE_GRAPH_MAX_CELLS];
    int vertex_col[MAZE_GRAPH_MAX_CELLS];
    MazeEdgeNode *adjacency[MAZE_GRAPH_MAX_CELLS];
} MazeGraph;

void MazeGraph_Init(MazeGraph *graph, int rows, int cols);
void MazeGraph_Free(MazeGraph *graph);
int MazeGraph_BuildFromGrid(MazeGraph *graph, const int *grid, int rows, int cols);
int MazeGraph_FindShortestPath(const MazeGraph *graph, int start_row, int start_col,
                               int goal_row, int goal_col, int *path, int max_path);
int MazeGraph_DfsReachableCount(const MazeGraph *graph, int start_row, int start_col);
int MazeGraph_VertexAt(const MazeGraph *graph, int row, int col);

#ifdef __cplusplus
}
#endif

#endif
