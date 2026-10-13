#include "maze_sim.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    MazeGraph g;
    int grid[4 * 31];
    for (int i = 0; i < 124; i++) grid[i] = 1;
    /* A 62-move snake corridor with no shortcuts between its two arms. */
    for (int c = 0; c < 31; c++) grid[c] = grid[62+c] = 0;
    grid[61] = 0;
    /* Test the exact stamina boundary through shared rules. */
    assert(MazeRules_Move(1) == 0);
    assert(MazeRules_Collect(95) == 100);
    assert(MazeRules_Collect(0) == 20);
    MazeGraph_Init(&g, 4, 31);
    assert(MazeGraph_BuildFromGrid(&g, grid, 4, 31));
    int start = MazeGraph_VertexAt(&g, 0, 0);
    int goal = MazeGraph_VertexAt(&g, 2, 0);
    assert(MazeSim_Run(&g, start, goal, NULL, 0, MAZE_BOT_GREEDY, 123) == 1);
    int a = MazeSim_Run(&g, start, goal, NULL, 0, MAZE_BOT_RANDOM, 123);
    assert(a == MazeSim_Run(&g, start, goal, NULL, 0, MAZE_BOT_RANDOM, 123));
    assert(MazeSim_Run(&g, start, start, NULL, 0, MAZE_BOT_RANDOM, 123) == 1);
    assert(MazeSim_Run(&g, -1, goal, NULL, 0, MAZE_BOT_RANDOM, 123) == -1);
    MazeGraph_Free(&g);

    /* Build a longer snake, all degree <=2: 126 moves, over the budget. */
    int long_grid[7 * 31];
    for (int i = 0; i < 217; i++) long_grid[i] = 1;
    for (int r = 0; r < 7; r += 2)
        for (int c = 0; c < 31; c++) long_grid[r*31+c] = 0;
    long_grid[1*31+30] = long_grid[3*31] = long_grid[5*31+30] = 0;
    assert(MazeGraph_BuildFromGrid(&g, long_grid, 7, 31));
    start = MazeGraph_VertexAt(&g, 0, 0);
    int hundredth = MazeGraph_VertexAt(&g, 6, 26);
    int hundred_first = MazeGraph_VertexAt(&g, 6, 25);
    assert(MazeSim_Run(&g, start, hundredth, NULL, 0, MAZE_BOT_GREEDY, 7) == 1);
    assert(MazeSim_Run(&g, start, hundred_first, NULL, 0, MAZE_BOT_GREEDY, 7) == 0);
    assert(MazeSim_Run(&g, start, hundred_first, &hundredth, 1, MAZE_BOT_GREEDY, 7) == 1);
    goal = MazeGraph_VertexAt(&g, 6, 0);
    assert(MazeSim_Run(&g, start, goal, NULL, 0, MAZE_BOT_GREEDY, 7) == 0);
    int fruit[] = {MazeGraph_VertexAt(&g, 4, 10), MazeGraph_VertexAt(&g, 6, 20)};
    assert(MazeSim_Run(&g, start, goal, fruit, 2, MAZE_BOT_GREEDY, 7) == 1);
    /* One fruit cannot pay for the whole route, even if revisited. */
    assert(MazeSim_Run(&g, start, goal, fruit, 1, MAZE_BOT_GREEDY, 7) == 0);
    int duplicate_fruit[] = {fruit[0], fruit[0]};
    assert(MazeSim_Run(&g, start, goal, duplicate_fruit, 2, MAZE_BOT_GREEDY, 7) == -1);
    MazeGraph_Free(&g);
    puts("Simulation tests PASS");
}
