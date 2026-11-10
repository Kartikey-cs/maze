#include "structs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_QUERY_WORDS 8

static void *Alloc(size_t bytes)
{
    void *memory = malloc(bytes > 0 ? bytes : 1);

    if (!memory)
    {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    return memory;
}

void RoadQueue_Init(RoadQueue *queue, int capacity)
{
    queue->items = (int *)Alloc((size_t)capacity * sizeof(int));
    queue->head = 0;
    queue->tail = 0;
    queue->capacity = capacity;
}

int RoadQueue_Push(RoadQueue *queue, int value)
{
    if (queue->tail >= queue->capacity)
        return 0;
    queue->items[queue->tail++] = value;
    return 1;
}

int RoadQueue_Pop(RoadQueue *queue, int *value)
{
    if (queue->head >= queue->tail)
        return 0;
    *value = queue->items[queue->head++];
    return 1;
}

void RoadQueue_Free(RoadQueue *queue)
{
    free(queue->items);
    queue->items = NULL;
}

void RoadStack_Init(RoadStack *stack, int capacity)
{
    stack->items = (int *)Alloc((size_t)capacity * sizeof(int));
    stack->size = 0;
    stack->capacity = capacity;
}

int RoadStack_Push(RoadStack *stack, int value)
{
    if (stack->size >= stack->capacity)
        return 0;
    stack->items[stack->size++] = value;
    return 1;
}

int RoadStack_Pop(RoadStack *stack, int *value)
{
    if (stack->size == 0)
        return 0;
    *value = stack->items[--stack->size];
    return 1;
}

void RoadStack_Free(RoadStack *stack)
{
    free(stack->items);
    stack->items = NULL;
}

void RoadHeap_Init(RoadHeap *heap, int capacity)
{
    heap->items = (RoadHeapItem *)Alloc((size_t)capacity * sizeof(RoadHeapItem));
    heap->size = 0;
    heap->capacity = capacity;
}

int RoadHeap_Push(RoadHeap *heap, double priority, int value)
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

int RoadHeap_Pop(RoadHeap *heap, int *value)
{
    if (heap->size == 0)
        return 0;

    *value = heap->items[0].value;

    /* Take the last item, put it at the top, then move it down. */
    RoadHeapItem last = heap->items[--heap->size];
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

void RoadHeap_Free(RoadHeap *heap)
{
    free(heap->items);
    heap->items = NULL;
}

/* The slot of a letter or digit in a trie node, or -1 if the character ends a word. */
static int Slot(char c)
{
    if (c >= 'a' && c <= 'z')
        return c - 'a';
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= '0' && c <= '9')
        return 26 + c - '0';
    return -1;
}

/* Gives a bigger array with the same items, twice the room. */
static void *Double(void *items, int *capacity, size_t size)
{
    void *bigger = realloc(items, (size_t)*capacity * 2 * size);

    if (!bigger)
    {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    *capacity *= 2;
    return bigger;
}

static int NewNode(Trie *trie)
{
    if (trie->node_count == trie->node_capacity)
        trie->nodes = (TrieNode *)Double(trie->nodes, &trie->node_capacity, sizeof(TrieNode));

    TrieNode *node = &trie->nodes[trie->node_count];

    memset(node->child, 0, sizeof node->child);
    node->first_hit = -1;
    return trie->node_count++;
}

void Trie_Init(Trie *trie)
{
    trie->node_capacity = 1024;
    trie->hit_capacity = 1024;
    trie->nodes = (TrieNode *)Alloc((size_t)trie->node_capacity * sizeof(TrieNode));
    trie->hits = (TrieHit *)Alloc((size_t)trie->hit_capacity * sizeof(TrieHit));
    trie->node_count = 0;
    trie->hit_count = 0;
    trie->place_count = 0;
    NewNode(trie);              /* the root */
}

void Trie_Free(Trie *trie)
{
    free(trie->nodes);
    free(trie->hits);
    memset(trie, 0, sizeof *trie);
}

static void AddHit(Trie *trie, int node, int place)
{
    int top = trie->nodes[node].first_hit;

    if (top >= 0 && trie->hits[top].place == place)
        return;                 /* the same word twice in one name */
    if (trie->hit_count == trie->hit_capacity)
        trie->hits = (TrieHit *)Double(trie->hits, &trie->hit_capacity, sizeof(TrieHit));
    trie->hits[trie->hit_count].place = place;
    trie->hits[trie->hit_count].next = top;
    trie->nodes[node].first_hit = trie->hit_count++;
}

void Trie_Add(Trie *trie, const char *name, int place)
{
    const char *p = name;

    if (place >= trie->place_count)
        trie->place_count = place + 1;

    while (*p)
    {
        if (Slot(*p) < 0)
        {
            p++;
            continue;
        }

        int node = 0;
        for (; Slot(*p) >= 0; p++)
        {
            int slot = Slot(*p);

            if (trie->nodes[node].child[slot] == 0)
            {
                int made = NewNode(trie);       /* can move the nodes, so use the index after it */
                trie->nodes[node].child[slot] = made;
            }
            node = trie->nodes[node].child[slot];
        }
        AddHit(trie, node, place);
    }
}

int Trie_Find(const Trie *trie, const char *query, int *found, int max_found)
{
    if (trie->place_count == 0 || max_found <= 0)
        return 0;

    /* level[p] is how many words of the query place p has matched so far. */
    unsigned char *level = (unsigned char *)Alloc((size_t)trie->place_count);
    int *stack = (int *)Alloc((size_t)trie->node_count * sizeof(int));
    int words = 0;
    int count = 0;
    const char *p = query;

    memset(level, 0, (size_t)trie->place_count);
    while (*p && words < MAX_QUERY_WORDS)
    {
        if (Slot(*p) < 0)
        {
            p++;
            continue;
        }

        int node = 0;
        for (; Slot(*p) >= 0; p++)
        {
            if (node >= 0)
                node = trie->nodes[node].child[Slot(*p)] ? trie->nodes[node].child[Slot(*p)] : -1;
        }
        if (node < 0)
        {
            words = 0;          /* no name has a word like this: nothing matches */
            break;
        }
        words++;

        /* Every place below this node has a word that starts with the typed letters. */
        int top = 0;
        stack[top++] = node;
        while (top > 0)
        {
            int n = stack[--top];

            for (int h = trie->nodes[n].first_hit; h >= 0; h = trie->hits[h].next)
            {
                if (level[trie->hits[h].place] == words - 1)
                    level[trie->hits[h].place] = (unsigned char)words;
            }
            for (int s = 0; s < TRIE_LETTERS; s++)
            {
                if (trie->nodes[n].child[s])
                    stack[top++] = trie->nodes[n].child[s];
            }
        }
    }

    for (int place = 0; words > 0 && place < trie->place_count && count < max_found; place++)
    {
        if (level[place] == words)
            found[count++] = place;
    }
    free(level);
    free(stack);
    return count;
}
