#ifndef STRUCTS_H
#define STRUCTS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Our own queue, stack, min-heap and trie, in C. Init takes all the room at once and stops
 * the program if there is no memory. Push returns 0 when full, Pop returns 0 when empty. */

typedef struct {
    int *items;
    int head;               /* where the next Pop reads */
    int tail;               /* where the next Push writes */
    int capacity;
} RoadQueue;

void RoadQueue_Init(RoadQueue *queue, int capacity);
int RoadQueue_Push(RoadQueue *queue, int value);
int RoadQueue_Pop(RoadQueue *queue, int *value);
void RoadQueue_Free(RoadQueue *queue);

typedef struct {
    int *items;
    int size;
    int capacity;
} RoadStack;

void RoadStack_Init(RoadStack *stack, int capacity);
int RoadStack_Push(RoadStack *stack, int value);
int RoadStack_Pop(RoadStack *stack, int *value);
void RoadStack_Free(RoadStack *stack);

/* Pop gives back the value with the smallest priority. The heap is an array: the parent of
 * item i is item (i - 1) / 2. */
typedef struct {
    double priority;
    int value;
} RoadHeapItem;

typedef struct {
    RoadHeapItem *items;
    int size;
    int capacity;
} RoadHeap;

void RoadHeap_Init(RoadHeap *heap, int capacity);
int RoadHeap_Push(RoadHeap *heap, double priority, int value);
int RoadHeap_Pop(RoadHeap *heap, int *value);
void RoadHeap_Free(RoadHeap *heap);

/* A trie of the words in names: "raj" and "road" both find "Rajpur Road". Words are letters
 * and digits, capitals are the same as small letters, anything else separates words. */
#define TRIE_LETTERS 36

typedef struct {
    int child[TRIE_LETTERS];    /* 0 if none, the root is node 0 and is never a child */
    int first_hit;              /* -1 if no name has a word that ends here */
} TrieNode;

typedef struct {
    int place;
    int next;                   /* the next hit on the same node, or -1 */
} TrieHit;

typedef struct {
    TrieNode *nodes;
    int node_count;
    int node_capacity;
    TrieHit *hits;
    int hit_count;
    int hit_capacity;
    int place_count;            /* the highest place number added, plus one */
} Trie;

void Trie_Init(Trie *trie);
void Trie_Free(Trie *trie);

/* Adds every word of name, for place number `place`. */
void Trie_Add(Trie *trie, const char *name, int place);

/* Finds the places that have, for every word of the query, a word that starts with it:
 * "raj ro" finds "Rajpur Road". Writes at most max_found place numbers, in order, and returns
 * how many. An empty query finds nothing. */
int Trie_Find(const Trie *trie, const char *query, int *found, int max_found);

#ifdef __cplusplus
}
#endif

#endif
