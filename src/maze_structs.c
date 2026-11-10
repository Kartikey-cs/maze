#include "maze_structs.h"

#include <stdlib.h>

int MazeQueue_Init(MazeQueue *queue, int capacity)
{
    queue->items = (int *)malloc((size_t)capacity * sizeof(int));
    queue->head = 0;
    queue->tail = 0;
    queue->capacity = queue->items ? capacity : 0;
    return queue->items != NULL;
}

int MazeQueue_Push(MazeQueue *queue, int value)
{
    if (queue->tail >= queue->capacity)
        return 0;
    queue->items[queue->tail++] = value;
    return 1;
}

int MazeQueue_Pop(MazeQueue *queue, int *value)
{
    if (queue->head >= queue->tail)
        return 0;
    *value = queue->items[queue->head++];
    return 1;
}

void MazeQueue_Free(MazeQueue *queue)
{
    free(queue->items);
    queue->items = NULL;
}

int MazeStack_Init(MazeStack *stack, int capacity)
{
    stack->items = (int *)malloc((size_t)capacity * sizeof(int));
    stack->size = 0;
    stack->capacity = stack->items ? capacity : 0;
    return stack->items != NULL;
}

int MazeStack_Push(MazeStack *stack, int value)
{
    if (stack->size >= stack->capacity)
        return 0;
    stack->items[stack->size++] = value;
    return 1;
}

int MazeStack_Pop(MazeStack *stack, int *value)
{
    if (stack->size == 0)
        return 0;
    *value = stack->items[--stack->size];
    return 1;
}

void MazeStack_Free(MazeStack *stack)
{
    free(stack->items);
    stack->items = NULL;
}

int MazeHeap_Init(MazeHeap *heap, int capacity)
{
    heap->items = (MazeHeapItem *)malloc((size_t)capacity * sizeof(MazeHeapItem));
    heap->size = 0;
    heap->capacity = heap->items ? capacity : 0;
    return heap->items != NULL;
}

int MazeHeap_Push(MazeHeap *heap, int priority, int value)
{
    if (heap->size >= heap->capacity)
        return 0;

    /* Put the new item at the end, then move it up while it beats its parent. */
    int i = heap->size++;
    while (i > 0 && heap->items[(i - 1) / 2].priority > priority)
    {
        heap->items[i] = heap->items[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    heap->items[i].priority = priority;
    heap->items[i].value = value;
    return 1;
}

int MazeHeap_Pop(MazeHeap *heap, int *value)
{
    if (heap->size == 0)
        return 0;

    *value = heap->items[0].value;

    /* Take the last item, put it at the top, then move it down. */
    MazeHeapItem last = heap->items[--heap->size];
    int i = 0;
    while (2 * i + 1 < heap->size)
    {
        int child = 2 * i + 1;
        if (child + 1 < heap->size && heap->items[child + 1].priority < heap->items[child].priority)
            child++;
        if (last.priority <= heap->items[child].priority)
            break;
        heap->items[i] = heap->items[child];
        i = child;
    }
    if (heap->size > 0)
        heap->items[i] = last;
    return 1;
}

void MazeHeap_Free(MazeHeap *heap)
{
    free(heap->items);
    heap->items = NULL;
}
