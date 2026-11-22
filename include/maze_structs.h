/* maze_structs.h - our own queue, stack and min-heap of ints (maze_structs.c) */
#ifndef MAZE_STRUCTS_H
#define MAZE_STRUCTS_H

/* the game is C++, so tell it these are C functions */
#ifdef __cplusplus
extern "C" {
#endif

/* init takes all the room at once (returns 0 if there is no memory).
   push returns 0 when full, pop returns 0 when empty. */

typedef struct {
    int *items;
    int head;       /* where the next pop reads */
    int tail;       /* where the next push writes */
    int capacity;
} Queue;

int queue_init(Queue *q, int capacity);
int queue_push(Queue *q, int value);
int queue_pop(Queue *q, int *value);
void queue_free(Queue *q);

typedef struct {
    int *items;
    int size;
    int capacity;
} Stack;

int stack_init(Stack *s, int capacity);
int stack_push(Stack *s, int value);
int stack_pop(Stack *s, int *value);
void stack_free(Stack *s);

/* pop gives back the value with the smallest priority.
   it is stored in an array: the parent of item i is item (i - 1) / 2 */
typedef struct {
    int priority;
    int value;
} HeapItem;

typedef struct {
    HeapItem *items;
    int size;
    int capacity;
} Heap;

int heap_init(Heap *h, int capacity);
int heap_push(Heap *h, int priority, int value);
int heap_pop(Heap *h, int *value);
void heap_free(Heap *h);

#ifdef __cplusplus
}
#endif

#endif
