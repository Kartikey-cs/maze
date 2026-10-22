#ifndef MAZE_HEAP_H
#define MAZE_HEAP_H

#include "maze_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Dijkstra and A* can queue a vertex once for every edge that reaches it. */
#define MAZE_HEAP_MAX (4 * MAZE_GRAPH_MAX_CELLS)

typedef struct {
    int priority;
    int vertex;
} MazeHeapItem;

/* A min-heap: Pop always gives back the item with the smallest priority.
 * It lives in an array, so the parent of item i is item (i - 1) / 2. */
typedef struct {
    MazeHeapItem items[MAZE_HEAP_MAX];
    int size;
} MazeHeap;

void MazeHeap_Init(MazeHeap *heap);

/* Both return 1 on success. Push fails when the heap is full, Pop when it is empty. */
int MazeHeap_Push(MazeHeap *heap, int priority, int vertex);
int MazeHeap_Pop(MazeHeap *heap, MazeHeapItem *item);

#ifdef __cplusplus
}
#endif

#endif
