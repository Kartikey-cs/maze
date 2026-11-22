/* maze_structs.c - our own queue, stack and min-heap of ints */
#include "maze_structs.h"
#include <stdlib.h>

int queue_init(Queue *q, int capacity)
{
    q->items = (int *)malloc(capacity * sizeof(int));
    q->head = 0;
    q->tail = 0;
    q->capacity = 0;
    if (q->items == NULL)
        return 0;
    q->capacity = capacity;
    return 1;
}

int queue_push(Queue *q, int value)
{
    if (q->tail >= q->capacity)   /* full */
        return 0;
    q->items[q->tail] = value;
    q->tail++;
    return 1;
}

int queue_pop(Queue *q, int *value)
{
    if (q->head >= q->tail)       /* empty */
        return 0;
    *value = q->items[q->head];
    q->head++;
    return 1;
}

void queue_free(Queue *q)
{
    free(q->items);
    q->items = NULL;
}

int stack_init(Stack *s, int capacity)
{
    s->items = (int *)malloc(capacity * sizeof(int));
    s->size = 0;
    s->capacity = 0;
    if (s->items == NULL)
        return 0;
    s->capacity = capacity;
    return 1;
}

int stack_push(Stack *s, int value)
{
    if (s->size >= s->capacity)
        return 0;
    s->items[s->size] = value;
    s->size++;
    return 1;
}

int stack_pop(Stack *s, int *value)
{
    if (s->size == 0)
        return 0;
    s->size--;
    *value = s->items[s->size];
    return 1;
}

void stack_free(Stack *s)
{
    free(s->items);
    s->items = NULL;
}

int heap_init(Heap *h, int capacity)
{
    h->items = (HeapItem *)malloc(capacity * sizeof(HeapItem));
    h->size = 0;
    h->capacity = 0;
    if (h->items == NULL)
        return 0;
    h->capacity = capacity;
    return 1;
}

int heap_push(Heap *h, int priority, int value)
{
    if (h->size >= h->capacity)
        return 0;

    /* put the new item at the end, then move it up while its parent is bigger */
    int i = h->size;
    h->size++;
    while (i > 0 && h->items[(i - 1) / 2].priority > priority)
    {
        h->items[i] = h->items[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    h->items[i].priority = priority;
    h->items[i].value = value;
    return 1;
}

int heap_pop(Heap *h, int *value)
{
    if (h->size == 0)
        return 0;

    *value = h->items[0].value;

    /* take the last item, put it at the top, then move it down */
    h->size--;
    HeapItem last = h->items[h->size];
    int i = 0;
    while (2 * i + 1 < h->size)
    {
        int child = 2 * i + 1;   /* left child */
        if (child + 1 < h->size && h->items[child + 1].priority < h->items[child].priority)
            child++;             /* right child is smaller */
        if (last.priority <= h->items[child].priority)
            break;
        h->items[i] = h->items[child];
        i = child;
    }
    if (h->size > 0)
        h->items[i] = last;
    return 1;
}

void heap_free(Heap *h)
{
    free(h->items);
    h->items = NULL;
}
