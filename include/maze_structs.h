#ifndef MAZE_STRUCTS_H
#define MAZE_STRUCTS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Queue, stack and min-heap of ints. Each one gets its room once, in Init (it returns 0
 * if there is no memory). Push returns 0 when full, Pop returns 0 when empty. */

/* Queue: first in, first out. */
typedef struct {
    int *items;
    int head;               /* where the next Pop reads */
    int tail;               /* where the next Push writes */
    int capacity;           /* total pushes it can take */
} MazeQueue;

int MazeQueue_Init(MazeQueue *queue, int capacity);
int MazeQueue_Push(MazeQueue *queue, int value);
int MazeQueue_Pop(MazeQueue *queue, int *value);
void MazeQueue_Free(MazeQueue *queue);

/* Stack: last in, first out. */
typedef struct {
    int *items;
    int size;
    int capacity;
} MazeStack;

int MazeStack_Init(MazeStack *stack, int capacity);
int MazeStack_Push(MazeStack *stack, int value);
int MazeStack_Pop(MazeStack *stack, int *value);
void MazeStack_Free(MazeStack *stack);

/* Min-heap: Pop gives back the value with the smallest priority. It lives in an array,
 * so the parent of item i is item (i - 1) / 2. */
typedef struct {
    int priority;
    int value;
} MazeHeapItem;

typedef struct {
    MazeHeapItem *items;
    int size;
    int capacity;
} MazeHeap;

int MazeHeap_Init(MazeHeap *heap, int capacity);
int MazeHeap_Push(MazeHeap *heap, int priority, int value);
int MazeHeap_Pop(MazeHeap *heap, int *value);
void MazeHeap_Free(MazeHeap *heap);

#ifdef __cplusplus
}
#endif

#endif
