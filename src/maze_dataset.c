#include "maze_features.h"
#include "maze_generator.h"
#include "maze_rng.h"
#include "maze_sim.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int ParsePositive(const char *text, unsigned long max, unsigned long *out)
{
    char *end;
    errno = 0;
    unsigned long value = strtoul(text, &end, 10);
    if (errno || end == text || *end || text[0] == '-' || value == 0 || value > max) return 0;
    *out = value;
    return 1;
}

int main(int argc, char **argv)
{
    unsigned long seed_count = 200, trials = 64, first_seed = 1;
    if (argc > 4 || (argc > 1 && !ParsePositive(argv[1], 100000, &seed_count)) ||
        (argc > 2 && !ParsePositive(argv[2], 100000, &trials)) ||
        (argc > 3 && !ParsePositive(argv[3], UINT32_MAX, &first_seed)) ||
        (uint64_t)first_seed + seed_count - 1 > UINT32_MAX)
    {
        fprintf(stderr, "Usage: %s [seed_count=200] [trials_per_bot=64] [first_seed=1]\n", argv[0]);
        return 1;
    }
    puts("seed,difficulty,fruit_count,trials_per_bot,vertices,edges,dead_ends,junctions,loops,shortest_path,detour_ratio,random_successes,greedy_successes,success_rate");
    for (unsigned long offset = 0; offset < seed_count; offset++)
    {
        unsigned long seed = first_seed + offset;
        for (int difficulty = 0; difficulty < 3; difficulty++)
        {
            MazeDifficultyParams params;
            MazeGen_GetParams((MazeDifficulty)difficulty, &params);
            int grid[MAZE_GRAPH_MAX_CELLS], candidates[MAZE_GRAPH_MAX_CELLS];
            MazeGraph graph;
            MazeFeatures features;
            MazeGraph_Init(&graph, params.rows, params.cols);
            if (!MazeGen_Generate(grid, params.rows, params.cols, params.loop_percent, (unsigned int)seed) ||
                !MazeGraph_BuildFromGrid(&graph, grid, params.rows, params.cols) ||
                !MazeFeatures_Extract(&graph, 1, 1, params.rows-2, params.cols-2, &features))
            {
                MazeGraph_Free(&graph);
                fprintf(stderr, "Failed maze seed=%lu difficulty=%d\n", seed, difficulty);
                return 1;
            }
            int start = MazeGraph_VertexAt(&graph, 1, 1);
            int goal = MazeGraph_VertexAt(&graph, params.rows-2, params.cols-2);
            int count = 0;
            for (int v = 0; v < graph.vertex_count; v++)
                if (v != start && v != goal) candidates[count++] = v;
            uint32_t rng = (uint32_t)seed ^ ((uint32_t)difficulty + 1) * UINT32_C(0x9e3779b9);
            for (int i = count-1; i > 0; i--)
            {
                int j = (int)(MazeRng_Next(&rng) % (uint32_t)(i+1));
                int swap = candidates[i]; candidates[i] = candidates[j]; candidates[j] = swap;
            }
            /* One fixed seeded fruit layout shared by every trial and bot. */
            int successes[2] = {0};
            for (int bot = 0; bot < 2; bot++)
                for (unsigned long trial = 0; trial < trials; trial++)
                {
                    int outcome = MazeSim_Run(&graph, start, goal, candidates, params.fruit_count,
                                              (MazeBot)bot, MazeRng_Next(&rng));
                    if (outcome < 0) { MazeGraph_Free(&graph); return 1; }
                    successes[bot] += outcome;
                }
            printf("%lu,%d,%d,%lu,%d,%d,%d,%d,%d,%d,%.17g,%d,%d,%.17g\n",
                   seed, difficulty, params.fruit_count, trials, features.vertices, features.edges,
                   features.dead_ends, features.junctions, features.loops, features.shortest_path,
                   features.detour_ratio, successes[0], successes[1],
                   (successes[0] + successes[1]) / (2.0 * trials));
            MazeGraph_Free(&graph);
        }
    }
    return ferror(stdout) ? 1 : 0;
}
