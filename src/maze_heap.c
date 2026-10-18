#include "maze_heap.h"

void MazeHeap_Init(MazeHeap *heap)
{
    heap->size = 0;
}

int MazeHeap_Push(MazeHeap *heap, int priority, int vertex)
{
    if (heap->size >= MAZE_HEAP_MAX)
        return 0;

    /* Put the new item at the end, then move it up while it beats its parent. */
    int i = heap->size++;
    while (i > 0)
    {
        int parent = (i - 1) / 2;
        if (heap->items[parent].priority <= priority)
            break;
        heap->items[i] = heap->items[parent];
        i = parent;
    }
    heap->items[i].priority = priority;
    heap->items[i].vertex = vertex;
    return 1;
}

int MazeHeap_Pop(MazeHeap *heap, MazeHeapItem *item)
{
    if (heap->size == 0)
        return 0;

    *item = heap->items[0];

    /* Take the last item, put it at the top, then move it down. */
    MazeHeapItem last = heap->items[--heap->size];
    int i = 0;
    while (1)
    {
        int child = 2 * i + 1;
        if (child >= heap->size)
            break;
        if (child + 1 < heap->size && heap->items[child + 1].priority < heap->items[child].priority)
            child++;                        /* the smaller of the two children */
        if (last.priority <= heap->items[child].priority)
            break;
        heap->items[i] = heap->items[child];
        i = child;
    }
    if (heap->size > 0)
        heap->items[i] = last;
    return 1;
}
