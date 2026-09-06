#include "maze_generator.h"
#include "maze_graph.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

void MazeGen_GetParams(MazeDifficulty diff, MazeDifficultyParams *out)
{
    switch (diff)
    {
    case MAZE_DIFFICULTY_EASY:
        out->rows         = 15;
        out->cols         = 15;
        out->loop_percent = 15;
        out->fruit_count  = 5;
        break;
    case MAZE_DIFFICULTY_HARD:
        out->rows         = 31;
        out->cols         = 31;
        out->loop_percent = 0;
        out->fruit_count  = 3;
        break;
    case MAZE_DIFFICULTY_MEDIUM:
    default:
        out->rows         = 21;
        out->cols         = 21;
        out->loop_percent = 5;
        out->fruit_count  = 4;
        break;
    }
}

typedef struct { unsigned int state; } Xorshift32;

static unsigned int xorshift32_next(Xorshift32 *rng)
{
    unsigned int x = rng->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng->state = x;
    return x;
}

static void shuffle(int *arr, int n, Xorshift32 *rng)
{
    for (int i = n - 1; i > 0; i--)
    {
        int j = (int)(xorshift32_next(rng) % (unsigned int)(i + 1));
        int tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
}

int MazeGen_Generate(int *grid, int rows, int cols, int loop_percent, unsigned int seed)
{
    if (rows < 3 || cols < 3) return 0;
    if ((rows & 1) == 0 || (cols & 1) == 0) return 0;
    if (rows > MAZE_GRAPH_MAX_ROWS || cols > MAZE_GRAPH_MAX_COLS) return 0;
    if (loop_percent < 0) loop_percent = 0;
    if (loop_percent > 100) loop_percent = 100;

    Xorshift32 rng;
    rng.state = (seed != 0) ? seed : (unsigned int)time(NULL);
    if (rng.state == 0) rng.state = 1;

    int total = rows * cols;

    for (int i = 0; i < total; i++)
        grid[i] = 1;

    int cell_rows = (rows - 1) / 2;
    int cell_cols = (cols - 1) / 2;

    int visited[MAZE_GRAPH_MAX_ROWS * MAZE_GRAPH_MAX_COLS];
    memset(visited, 0, sizeof(visited));

    int stack[MAZE_GRAPH_MAX_ROWS * MAZE_GRAPH_MAX_COLS];
    int top = 0;

    const int dcr[4] = {-1, 1,  0, 0};
    const int dcc[4] = { 0, 0, -1, 1};

    int start_idx = 0;
    visited[start_idx] = 1;
    grid[1 * cols + 1] = 0;
    stack[top++] = start_idx;

    while (top > 0)
    {
        int cur = stack[top - 1];
        int cr = cur / cell_cols;
        int cc = cur % cell_cols;

        int dir_order[4] = {0, 1, 2, 3};
        shuffle(dir_order, 4, &rng);

        int found = 0;
        for (int d = 0; d < 4; d++)
        {
            int nd = dir_order[d];
            int nr = cr + dcr[nd];
            int nc = cc + dcc[nd];
            if (nr < 0 || nr >= cell_rows || nc < 0 || nc >= cell_cols) continue;

            int nidx = nr * cell_cols + nc;
            if (visited[nidx]) continue;

            int wall_row = cr * 2 + 1 + dcr[nd];
            int wall_col = cc * 2 + 1 + dcc[nd];
            grid[wall_row * cols + wall_col] = 0;

            int cell_row = nr * 2 + 1;
            int cell_col = nc * 2 + 1;
            grid[cell_row * cols + cell_col] = 0;

            visited[nidx] = 1;
            stack[top++] = nidx;
            found = 1;
            break;
        }

        if (!found)
            top--;
    }

    if (loop_percent > 0)
    {
        int candidates[MAZE_GRAPH_MAX_CELLS];
        int cand_count = 0;

        for (int r = 1; r < rows - 1; r++)
        {
            for (int c = 1; c < cols - 1; c++)
            {
                if (grid[r * cols + c] != 1) continue;

                int r_even = ((r & 1) == 0);
                int c_even = ((c & 1) == 0);

                if (r_even && !c_even)
                {
                    if (grid[(r - 1) * cols + c] == 0 && grid[(r + 1) * cols + c] == 0)
                    {
                        if (cand_count < MAZE_GRAPH_MAX_CELLS)
                            candidates[cand_count++] = r * cols + c;
                    }
                }
                else if (!r_even && c_even)
                {
                    if (grid[r * cols + (c - 1)] == 0 && grid[r * cols + (c + 1)] == 0)
                    {
                        if (cand_count < MAZE_GRAPH_MAX_CELLS)
                            candidates[cand_count++] = r * cols + c;
                    }
                }
            }
        }

        shuffle(candidates, cand_count, &rng);

        int to_remove = cand_count * loop_percent / 100;
        for (int i = 0; i < to_remove && i < cand_count; i++)
            grid[candidates[i]] = 0;
    }

    return 1;
}
