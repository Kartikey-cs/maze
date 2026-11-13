/* maze.h - everything shared by the maze programs */
#ifndef MAZE_H
#define MAZE_H

#include <stdint.h>

/* the game is C++, so tell it these are C functions */
#ifdef __cplusplus
extern "C" {
#endif

#define MAX_ROWS 32
#define MAX_COLS 32
#define MAX_CELLS (MAX_ROWS * MAX_COLS)

#define STAMINA_MAX 100
#define FRUIT_STAMINA 20

/* ---------- maze generator (maze_generator.c) ---------- */

typedef enum { EASY, MEDIUM, HARD } Difficulty;

typedef struct {
    int rows;
    int cols;
    int loop_percent;   /* how many extra walls to break to make loops */
    int fruit_count;
} Level;

Level get_level(Difficulty diff);

/* fills grid with 1 = wall and 0 = path. returns 0 if the size is wrong */
int generate_maze(int *grid, int rows, int cols, int loop_percent, unsigned int seed);

/* ---------- graph (maze_core.c) ---------- */

/* one node of the adjacency list */
typedef struct Edge {
    int to;
    struct Edge *next;
} Edge;

typedef struct {
    int rows;
    int cols;
    int vertex_count;
    int cell_to_vertex[MAX_ROWS][MAX_COLS];   /* -1 if the cell is a wall */
    int vertex_row[MAX_CELLS];
    int vertex_col[MAX_CELLS];
    Edge *adjacency[MAX_CELLS];
} Graph;

void graph_init(Graph *g, int rows, int cols);
void graph_free(Graph *g);
int build_graph(Graph *g, const int *grid, int rows, int cols);
int vertex_at(const Graph *g, int row, int col);
int bfs_shortest_path(const Graph *g, int start_row, int start_col,
                      int goal_row, int goal_col, int *path, int max_path);
int dfs_count(const Graph *g, int start_row, int start_col);

/* ---------- queue, stack and min-heap of ints (maze_core.c) ----------
   init takes all the room at once (returns 0 if there is no memory).
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

/* ---------- features for the model (maze_core.c) ---------- */

#define FEATURE_COUNT 7

typedef struct {
    int vertices;
    int edges;
    int dead_ends;
    int junctions;
    int loops;
    int shortest_path;
    double detour_ratio;
} Features;

int get_features(const Graph *g, int start_row, int start_col,
                 int goal_row, int goal_col, Features *f);
void features_to_array(const Features *f, double out[FEATURE_COUNT]);

/* ---------- bots and game rules (maze_core.c) ---------- */

#define BOT_RANDOM 0
#define BOT_GREEDY 1

uint32_t next_random(uint32_t *state);
int stamina_after_move(int stamina);
int stamina_after_fruit(int stamina);

/* returns 1 = reached goal, 0 = failed, -1 = wrong input */
int run_bot(const Graph *g, int start, int goal, const int *fruits,
            int fruit_count, int bot, uint32_t seed);

/* ---------- planner and adaptive mazes (maze_core.c) ---------- */

#define MAX_FRUITS 6

/* picks count different cells for fruit (never start or goal).
   the same seed always gives the same fruit. returns how many were placed */
int place_fruits(const Graph *g, int start, int goal, int count, uint32_t seed, int *fruits);

/* shortest route that wins with the stamina rules, starting with this much stamina.
   fruits are vertex numbers (each eaten once). path needs room for MAX_CELLS.
   returns how many cells the route has, or 0 if it can't be won */
int plan_route(const Graph *g, int start, int goal, const int *fruits, int fruit_count,
               int stamina, int *path);

typedef struct {
    Difficulty difficulty;
    unsigned int seed;          /* difficulty + seed gives back the exact maze */
    double predicted_success;   /* the model's guess: higher means easier */
} MazeChoice;

/* picks a winnable maze that fits the player's rating. returns 0 if none can be won */
int choose_adaptive_maze(double rating, unsigned int base_seed, MazeChoice *choice);

/* ---------- accounts, ratings and saved games (maze_users.c) ---------- */

#define MAX_LEN 64              /* longest username / password + 1 */
#define START_RATING 1000
#define LEADERBOARD_SIZE 10

/* both return 1 if it worked, and write a short message for the screen */
int signup_user(const char *username, const char *password, char *message, int size);
int login_user(const char *username, const char *password, char *message, int size);

double expected_score(double player, double maze);           /* chance (0 to 1) to win */
double new_rating(double player, double maze, int won);
double difficulty_rating(int difficulty);                    /* 0 easy, 1 medium, 2 hard */

typedef struct {
    char username[MAX_LEN];
    double rating;
    int games;
    int wins;
} LeaderboardRow;

int get_rating(const char *username, double *rating);         /* 1 if found */
/* saves one finished game and updates the rating. returns 1 if it worked */
int save_game(const char *username, int difficulty, int rows, int cols,
              int won, int moves, int stamina_left, double *rating_after);
int get_leaderboard(LeaderboardRow *rows, int max);           /* how many rows, or -1 */
int export_csv(const char *path);                             /* how many games, or -1 */

#ifdef __cplusplus
}
#endif

#endif
