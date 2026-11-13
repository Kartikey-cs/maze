/* maze_core.c - maze graph (BFS, DFS), queue/stack/heap, features,
   the two bots, the route planner and the adaptive maze chooser */
#include "maze.h"
#include "maze_model.h"   /* the trained model, made by tools/train_model.py */
#include <math.h>
#include <stdlib.h>

/* ================= random numbers ================= */

/* xorshift: a simple random number generator.
   we use our own instead of rand() so the same seed gives the
   same maze on every computer */
uint32_t next_random(uint32_t *state)
{
    uint32_t x = *state;
    if (x == 0)
        x = 1;
    x = x ^ (x << 13);
    x = x ^ (x >> 17);
    x = x ^ (x << 5);
    *state = x;
    return x;
}

/* ================= game rules ================= */

int stamina_after_move(int stamina)
{
    if (stamina > 0)
        return stamina - 1;
    return 0;
}

int stamina_after_fruit(int stamina)
{
    stamina = stamina + FRUIT_STAMINA;
    if (stamina > STAMINA_MAX)
        stamina = STAMINA_MAX;
    return stamina;
}

/* ================= graph ================= */

void graph_init(Graph *g, int rows, int cols)
{
    int i, j;
    g->rows = rows;
    g->cols = cols;
    g->vertex_count = 0;

    for (i = 0; i < MAX_ROWS; i++)
        for (j = 0; j < MAX_COLS; j++)
            g->cell_to_vertex[i][j] = -1;

    for (i = 0; i < MAX_CELLS; i++)
    {
        g->vertex_row[i] = -1;
        g->vertex_col[i] = -1;
        g->adjacency[i] = NULL;
    }
}

void graph_free(Graph *g)
{
    int i;
    for (i = 0; i < g->vertex_count; i++)
    {
        Edge *e = g->adjacency[i];
        while (e != NULL)
        {
            Edge *next = e->next;
            free(e);
            e = next;
        }
        g->adjacency[i] = NULL;
    }
    g->vertex_count = 0;
}

/* add edge from -> to at the front of the list */
static int add_edge(Graph *g, int from, int to)
{
    Edge *e = (Edge *)malloc(sizeof(Edge));
    if (e == NULL)
        return 0;
    e->to = to;
    e->next = g->adjacency[from];
    g->adjacency[from] = e;
    return 1;
}

/* every open cell (0) becomes a vertex,
   and two open cells next to each other get an edge */
int build_graph(Graph *g, const int *grid, int rows, int cols)
{
    int dr[4] = {-1, 1, 0, 0};   /* up, down, left, right */
    int dc[4] = {0, 0, -1, 1};
    int r, c, d;

    if (rows <= 0 || cols <= 0 || rows > MAX_ROWS || cols > MAX_COLS)
        return 0;

    graph_free(g);
    graph_init(g, rows, cols);

    /* step 1: give a number to every open cell */
    for (r = 0; r < rows; r++)
    {
        for (c = 0; c < cols; c++)
        {
            if (grid[r * cols + c] == 0)
            {
                int v = g->vertex_count;
                g->cell_to_vertex[r][c] = v;
                g->vertex_row[v] = r;
                g->vertex_col[v] = c;
                g->vertex_count++;
            }
        }
    }

    /* step 2: connect neighbours */
    for (r = 0; r < rows; r++)
    {
        for (c = 0; c < cols; c++)
        {
            int from = g->cell_to_vertex[r][c];
            if (from == -1)
                continue;

            for (d = 0; d < 4; d++)
            {
                int nr = r + dr[d];
                int nc = c + dc[d];
                if (nr < 0 || nr >= rows || nc < 0 || nc >= cols)
                    continue;

                int to = g->cell_to_vertex[nr][nc];
                if (to != -1)
                {
                    if (add_edge(g, from, to) == 0)
                    {
                        graph_free(g);
                        return 0;
                    }
                }
            }
        }
    }
    return 1;
}

/* vertex number of a cell, or -1 */
int vertex_at(const Graph *g, int row, int col)
{
    if (row < 0 || row >= g->rows || col < 0 || col >= g->cols)
        return -1;
    return g->cell_to_vertex[row][col];
}

/* BFS using a queue. puts the path (start ... goal) in path[]
   and returns how many cells are in it, or 0 if there is no path */
int bfs_shortest_path(const Graph *g, int start_row, int start_col,
                      int goal_row, int goal_col, int *path, int max_path)
{
    int start = vertex_at(g, start_row, start_col);
    int goal = vertex_at(g, goal_row, goal_col);
    int queue[MAX_CELLS];
    int parent[MAX_CELLS];
    int visited[MAX_CELLS] = {0};
    int front = 0, back = 0;
    int i;

    if (start == -1 || goal == -1 || max_path <= 0)
        return 0;

    for (i = 0; i < g->vertex_count; i++)
        parent[i] = -1;

    visited[start] = 1;
    queue[back] = start;
    back++;

    while (front < back)
    {
        int current = queue[front];
        front++;
        if (current == goal)
            break;

        Edge *e;
        for (e = g->adjacency[current]; e != NULL; e = e->next)
        {
            if (visited[e->to] == 0)
            {
                visited[e->to] = 1;
                parent[e->to] = current;
                queue[back] = e->to;
                back++;
            }
        }
    }

    if (visited[goal] == 0)
        return 0;

    /* walk back from goal to start using parent[] */
    int reversed[MAX_CELLS];
    int count = 0;
    int at = goal;
    while (at != -1 && count < MAX_CELLS)
    {
        reversed[count] = at;
        count++;
        at = parent[at];
    }

    if (count > max_path)
        count = max_path;

    /* flip it so it goes start -> goal */
    for (i = 0; i < count; i++)
        path[i] = reversed[count - 1 - i];

    return count;
}

/* DFS using a stack. returns how many cells can be reached from start */
int dfs_count(const Graph *g, int start_row, int start_col)
{
    int start = vertex_at(g, start_row, start_col);
    int stack[MAX_CELLS];
    int visited[MAX_CELLS] = {0};
    int top = 0;
    int count = 0;

    if (start == -1)
        return 0;

    stack[top] = start;
    top++;
    visited[start] = 1;

    while (top > 0)
    {
        top--;
        int current = stack[top];
        count++;

        Edge *e;
        for (e = g->adjacency[current]; e != NULL; e = e->next)
        {
            if (visited[e->to] == 0)
            {
                visited[e->to] = 1;
                stack[top] = e->to;
                top++;
            }
        }
    }
    return count;
}

/* ================= features ================= */

/* works out the 7 numbers the model uses.
   returns 0 if the maze is not fully connected or the cells are wrong */
int get_features(const Graph *g, int sr, int sc, int gr, int gc, Features *f)
{
    int path[MAX_CELLS];
    int v;

    if (f == NULL)
        return 0;

    f->vertices = 0;
    f->edges = 0;
    f->dead_ends = 0;
    f->junctions = 0;
    f->loops = 0;
    f->shortest_path = 0;
    f->detour_ratio = 0;

    if (g == NULL || g->vertex_count <= 0)
        return 0;
    if (dfs_count(g, sr, sc) != g->vertex_count)   /* some cells cut off */
        return 0;

    int count = bfs_shortest_path(g, sr, sc, gr, gc, path, MAX_CELLS);
    if (count == 0)
        return 0;

    /* count neighbours (degree) of every vertex */
    int degree_sum = 0;
    for (v = 0; v < g->vertex_count; v++)
    {
        int degree = 0;
        Edge *e;
        for (e = g->adjacency[v]; e != NULL; e = e->next)
            degree++;

        degree_sum = degree_sum + degree;
        if (degree == 1)
            f->dead_ends++;
        if (degree >= 3)
            f->junctions++;
    }

    f->vertices = g->vertex_count;
    f->edges = degree_sum / 2;               /* every edge was counted twice */
    f->loops = f->edges - f->vertices + 1;
    f->shortest_path = count - 1;            /* moves, not cells */

    double distance = hypot(gr - sr, gc - sc);   /* straight line distance */
    if (distance > 0)
        f->detour_ratio = f->shortest_path / distance;
    else
        f->detour_ratio = 1.0;

    return 1;
}

void features_to_array(const Features *f, double out[FEATURE_COUNT])
{
    out[0] = f->vertices;
    out[1] = f->edges;
    out[2] = f->dead_ends;
    out[3] = f->junctions;
    out[4] = f->loops;
    out[5] = f->shortest_path;
    out[6] = f->detour_ratio;
}

/* ================= bots ================= */

static int manhattan(const Graph *g, int a, int b)
{
    return abs(g->vertex_row[a] - g->vertex_row[b]) +
           abs(g->vertex_col[a] - g->vertex_col[b]);
}

/* runs one bot from start to goal.
   returns 1 if it reaches the goal, 0 if stamina runs out or it is stuck,
   -1 if the input is wrong */
int run_bot(const Graph *g, int start, int goal, const int *fruits,
            int fruit_count, int bot, uint32_t seed)
{
    int has_fruit[MAX_CELLS] = {0};
    int visits[MAX_CELLS] = {0};
    int i;

    /* check the input first */
    if (g == NULL)
        return -1;
    if (start < 0 || start >= g->vertex_count)
        return -1;
    if (goal < 0 || goal >= g->vertex_count)
        return -1;
    if (fruit_count < 0 || fruit_count > g->vertex_count)
        return -1;
    if (fruit_count > 0 && fruits == NULL)
        return -1;
    if (bot != BOT_RANDOM && bot != BOT_GREEDY)
        return -1;

    /* mark where the fruits are */
    for (i = 0; i < fruit_count; i++)
    {
        int v = fruits[i];
        if (v < 0 || v >= g->vertex_count)
            return -1;
        if (has_fruit[v] == 1)   /* same fruit given two times */
            return -1;
        has_fruit[v] = 1;
    }

    int current = start;
    int stamina = STAMINA_MAX;
    visits[current] = 1;

    /* keep moving until we reach the goal or stamina is over */
    while (current != goal && stamina > 0)
    {
        int choices[4];
        int count = 0;
        Edge *e;

        if (bot == BOT_RANDOM)
        {
            /* random bot: every neighbour is a choice */
            for (e = g->adjacency[current]; e != NULL; e = e->next)
            {
                choices[count] = e->to;
                count++;
            }
        }
        else
        {
            /* greedy bot step 1: find the least visited neighbour,
               and if there is a tie, the one closest to the goal */
            int best_visits = -1;
            int best_distance = -1;
            for (e = g->adjacency[current]; e != NULL; e = e->next)
            {
                int v = e->to;
                int distance = manhattan(g, v, goal);
                if (best_visits == -1 || visits[v] < best_visits ||
                    (visits[v] == best_visits && distance < best_distance))
                {
                    best_visits = visits[v];
                    best_distance = distance;
                }
            }

            /* greedy bot step 2: all neighbours that are equally best */
            for (e = g->adjacency[current]; e != NULL; e = e->next)
            {
                int v = e->to;
                if (visits[v] == best_visits && manhattan(g, v, goal) == best_distance)
                {
                    choices[count] = v;
                    count++;
                }
            }
        }

        if (count == 0)   /* no way to go */
            return 0;

        /* pick one of the choices randomly */
        int pick = next_random(&seed) % count;
        current = choices[pick];
        visits[current]++;

        /* every move costs stamina */
        stamina = stamina_after_move(stamina);

        /* eat the fruit if there is one here */
        if (has_fruit[current] == 1)
        {
            has_fruit[current] = 0;
            stamina = stamina_after_fruit(stamina);
        }
    }

    if (current == goal)
        return 1;
    return 0;
}

/* ================= queue, stack, heap ================= */

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

/* ================= planner ================= */

#define STAMINA_LEVELS (STAMINA_MAX + 1)   /* stamina can be 0 to 100 */
#define CANDIDATES 12                      /* mazes tried by the adaptive chooser */
#define EASIEST_RATING 800.0
#define HARDEST_RATING 1200.0

int place_fruits(const Graph *g, int start, int goal, int count, uint32_t seed, int *fruits)
{
    int cells[MAX_CELLS];
    int total = 0;
    int i, v;
    uint32_t rng = seed ^ 0x9e3779b9u;   /* mix the seed a bit */

    for (v = 0; v < g->vertex_count; v++)
    {
        if (v != start && v != goal)
        {
            cells[total] = v;
            total++;
        }
    }
    if (count > total)
        count = total;

    /* shuffle only the first count cells (enough to pick count different ones) */
    for (i = 0; i < count; i++)
    {
        int j = i + next_random(&rng) % (total - i);
        int temp = cells[i];
        cells[i] = cells[j];
        cells[j] = temp;
        fruits[i] = cells[i];
    }
    return count;
}

/*
 * BFS where a "state" is (cell, stamina left, which fruits are eaten).
 * The three numbers are packed into one int:
 *     state = (cell * STAMINA_LEVELS + stamina) * masks + eaten
 * "eaten" uses one bit per fruit. The first time BFS reaches the goal
 * is the winning route with the fewest moves.
 */
int plan_route(const Graph *g, int start, int goal, const int *fruits, int fruit_count,
               int stamina, int *path)
{
    int fruit_bit[MAX_CELLS];   /* which bit belongs to the fruit on this cell, or -1 */
    int i, v;

    if (fruit_count > MAX_FRUITS || stamina < 0 || stamina > STAMINA_MAX)
        return 0;
    if (start == goal)
    {
        path[0] = start;
        return 1;
    }

    for (v = 0; v < g->vertex_count; v++)
        fruit_bit[v] = -1;
    for (i = 0; i < fruit_count; i++)
        fruit_bit[fruits[i]] = i;

    int masks = 1 << fruit_count;
    int states = g->vertex_count * STAMINA_LEVELS * masks;

    /* parent[state] = the state we came from, -1 = not reached yet */
    int *parent = (int *)malloc(states * sizeof(int));
    Queue queue;
    if (parent == NULL || !queue_init(&queue, states))   /* each state is queued once */
    {
        free(parent);
        return 0;
    }
    for (i = 0; i < states; i++)
        parent[i] = -1;

    int first = (start * STAMINA_LEVELS + stamina) * masks;
    int found = -1;
    int state;

    parent[first] = first;
    queue_push(&queue, first);

    while (found == -1 && queue_pop(&queue, &state))
    {
        /* unpack the state */
        int eaten = state % masks;
        int left = (state / masks) % STAMINA_LEVELS;
        int at = (state / masks) / STAMINA_LEVELS;

        if (left == 0)   /* can't move any more */
            continue;

        Edge *e;
        for (e = g->adjacency[at]; e != NULL && found == -1; e = e->next)
        {
            int new_left = stamina_after_move(left);
            int new_eaten = eaten;
            int bit = fruit_bit[e->to];

            /* fruit here that we haven't eaten yet */
            if (bit != -1 && (eaten & (1 << bit)) == 0)
            {
                new_eaten = new_eaten | (1 << bit);
                new_left = stamina_after_fruit(new_left);
            }

            int next = (e->to * STAMINA_LEVELS + new_left) * masks + new_eaten;
            if (parent[next] != -1)   /* already seen */
                continue;
            parent[next] = state;

            if (e->to == goal)
                found = next;   /* reaching the exit wins, even with 0 stamina */
            else
                queue_push(&queue, next);
        }
    }

    int length = 0;
    if (found != -1)
    {
        /* walk back from the goal to the start */
        state = found;
        while (1)
        {
            path[length] = (state / masks) / STAMINA_LEVELS;
            length++;
            if (state == first)
                break;
            state = parent[state];
        }
        /* it was built backwards, so flip it */
        for (i = 0; i < length / 2; i++)
        {
            int temp = path[i];
            path[i] = path[length - 1 - i];
            path[length - 1 - i] = temp;
        }
    }

    free(parent);
    queue_free(&queue);
    return length;
}

/* ================= adaptive mazes ================= */

/* builds one maze and scores it with the model.
   returns 1 (and fills choice) only if the maze can be won */
static int make_candidate(Difficulty difficulty, unsigned int seed, MazeChoice *choice)
{
    Level level = get_level(difficulty);
    Graph g;
    Features f;
    int grid[MAX_CELLS];
    int fruits[MAX_FRUITS];
    int path[MAX_CELLS];
    double arr[FEATURE_COUNT];
    int ok = 0;

    if (!generate_maze(grid, level.rows, level.cols, level.loop_percent, seed))
        return 0;

    graph_init(&g, level.rows, level.cols);
    if (build_graph(&g, grid, level.rows, level.cols))
    {
        int start = vertex_at(&g, 1, 1);
        int goal = vertex_at(&g, level.rows - 2, level.cols - 2);

        /* same fruit the game places for this seed */
        int count = place_fruits(&g, start, goal, level.fruit_count, seed, fruits);

        if (plan_route(&g, start, goal, fruits, count, STAMINA_MAX, path) > 0 &&
            get_features(&g, 1, 1, level.rows - 2, level.cols - 2, &f))
        {
            features_to_array(&f, arr);
            choice->difficulty = difficulty;
            choice->seed = seed;
            choice->predicted_success = MazeModel_PredictSuccess(arr);
            ok = 1;
        }
    }

    graph_free(&g);
    return ok;
}

/* for qsort: easiest maze (highest predicted success) first */
static int easiest_first(const void *a, const void *b)
{
    const MazeChoice *p = (const MazeChoice *)a;
    const MazeChoice *q = (const MazeChoice *)b;

    if (p->predicted_success > q->predicted_success)
        return -1;
    if (p->predicted_success < q->predicted_success)
        return 1;
    if (p->seed < q->seed)   /* same guess: lower seed first */
        return -1;
    return 1;
}

/* makes 12 mazes (easy, medium, hard, easy, ...), drops the ones that can't be won,
   sorts them easiest first and picks one based on the rating */
int choose_adaptive_maze(double rating, unsigned int base_seed, MazeChoice *choice)
{
    MazeChoice candidates[CANDIDATES];
    int count = 0;
    int i;

    for (i = 0; i < CANDIDATES; i++)
    {
        unsigned int seed = base_seed + (unsigned int)i * 7919u;
        if (seed == 0)
            seed = 1;   /* the generator uses the clock for seed 0 */

        if (make_candidate((Difficulty)(i % 3), seed, &candidates[count]))
            count++;
    }
    if (count == 0)
        return 0;

    qsort(candidates, count, sizeof(MazeChoice), easiest_first);

    /* rating 800 or less -> easiest maze, 1200 or more -> hardest */
    double target = (rating - EASIEST_RATING) / (HARDEST_RATING - EASIEST_RATING);
    if (target < 0.0)
        target = 0.0;
    if (target > 1.0)
        target = 1.0;

    *choice = candidates[(int)(target * (count - 1) + 0.5)];
    return 1;
}
