#include "maze_graph.h"

#include <stdlib.h>

static int IsInside(int row, int col, int rows, int cols)
{
    return row >= 0 && row < rows && col >= 0 && col < cols;
}

static int GridAt(const int *grid, int cols, int row, int col)
{
    return grid[row * cols + col];
}

static int AddEdge(MazeGraph *graph, int from, int to)
{
    MazeEdgeNode *edge = (MazeEdgeNode *)malloc(sizeof(MazeEdgeNode));
    if (!edge) return 0;

    edge->to = to;
    edge->next = graph->adjacency[from];
    graph->adjacency[from] = edge;
    return 1;
}

void MazeGraph_Init(MazeGraph *graph, int rows, int cols)
{
    graph->rows = rows;
    graph->cols = cols;
    graph->vertex_count = 0;

    for (int row = 0; row < MAZE_GRAPH_MAX_ROWS; row++)
    {
        for (int col = 0; col < MAZE_GRAPH_MAX_COLS; col++)
            graph->cell_to_vertex[row][col] = -1;
    }

    for (int i = 0; i < MAZE_GRAPH_MAX_CELLS; i++)
    {
        graph->vertex_row[i] = -1;
        graph->vertex_col[i] = -1;
        graph->adjacency[i] = NULL;
    }
}

void MazeGraph_Free(MazeGraph *graph)
{
    for (int i = 0; i < graph->vertex_count; i++)
    {
        MazeEdgeNode *edge = graph->adjacency[i];
        while (edge)
        {
            MazeEdgeNode *next = edge->next;
            free(edge);
            edge = next;
        }
        graph->adjacency[i] = NULL;
    }
    graph->vertex_count = 0;
}

int MazeGraph_BuildFromGrid(MazeGraph *graph, const int *grid, int rows, int cols)
{
    const int d_row[4] = {-1, 1, 0, 0};
    const int d_col[4] = {0, 0, -1, 1};

    if (rows <= 0 || cols <= 0 || rows > MAZE_GRAPH_MAX_ROWS || cols > MAZE_GRAPH_MAX_COLS)
        return 0;

    MazeGraph_Free(graph);
    MazeGraph_Init(graph, rows, cols);

    for (int row = 0; row < rows; row++)
    {
        for (int col = 0; col < cols; col++)
        {
            if (GridAt(grid, cols, row, col) == 0)
            {
                int vertex = graph->vertex_count++;
                graph->cell_to_vertex[row][col] = vertex;
                graph->vertex_row[vertex] = row;
                graph->vertex_col[vertex] = col;
            }
        }
    }

    for (int row = 0; row < rows; row++)
    {
        for (int col = 0; col < cols; col++)
        {
            int from = graph->cell_to_vertex[row][col];
            if (from < 0) continue;

            for (int direction = 0; direction < 4; direction++)
            {
                int next_row = row + d_row[direction];
                int next_col = col + d_col[direction];
                if (!IsInside(next_row, next_col, rows, cols)) continue;

                int to = graph->cell_to_vertex[next_row][next_col];
                if (to >= 0 && !AddEdge(graph, from, to))
                {
                    MazeGraph_Free(graph);
                    return 0;
                }
            }
        }
    }

    return 1;
}

int MazeGraph_VertexAt(const MazeGraph *graph, int row, int col)
{
    if (!IsInside(row, col, graph->rows, graph->cols)) return -1;
    return graph->cell_to_vertex[row][col];
}

int MazeGraph_FindShortestPath(const MazeGraph *graph, int start_row, int start_col,
                               int goal_row, int goal_col, int *path, int max_path)
{
    int start = MazeGraph_VertexAt(graph, start_row, start_col);
    int goal = MazeGraph_VertexAt(graph, goal_row, goal_col);
    int queue[MAZE_GRAPH_MAX_CELLS];
    int previous[MAZE_GRAPH_MAX_CELLS];
    int visited[MAZE_GRAPH_MAX_CELLS] = {0};
    int head = 0;
    int tail = 0;

    if (start < 0 || goal < 0 || max_path <= 0) return 0;

    for (int i = 0; i < graph->vertex_count; i++)
        previous[i] = -1;

    visited[start] = 1;
    queue[tail++] = start;

    while (head < tail)
    {
        int current = queue[head++];
        if (current == goal) break;

        for (MazeEdgeNode *edge = graph->adjacency[current]; edge; edge = edge->next)
        {
            if (!visited[edge->to])
            {
                visited[edge->to] = 1;
                previous[edge->to] = current;
                queue[tail++] = edge->to;
            }
        }
    }

    if (!visited[goal]) return 0;

    int reversed[MAZE_GRAPH_MAX_CELLS];
    int count = 0;
    for (int at = goal; at != -1 && count < MAZE_GRAPH_MAX_CELLS; at = previous[at])
        reversed[count++] = at;

    if (count > max_path) count = max_path;

    for (int i = 0; i < count; i++)
        path[i] = reversed[count - 1 - i];

    return count;
}

int MazeGraph_DfsReachableCount(const MazeGraph *graph, int start_row, int start_col)
{
    int start = MazeGraph_VertexAt(graph, start_row, start_col);
    int stack[MAZE_GRAPH_MAX_CELLS];
    int visited[MAZE_GRAPH_MAX_CELLS] = {0};
    int top = 0;
    int count = 0;

    if (start < 0) return 0;

    stack[top++] = start;
    visited[start] = 1;

    while (top > 0)
    {
        int current = stack[--top];
        count++;

        for (MazeEdgeNode *edge = graph->adjacency[current]; edge; edge = edge->next)
        {
            if (!visited[edge->to])
            {
                visited[edge->to] = 1;
                stack[top++] = edge->to;
            }
        }
    }

    return count;
}
