#include "maze_features.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    MazeGraph g;
    MazeFeatures f;
    MazeGraph_Init(&g, 1, 3);
    const int corridor[] = {0, 0, 0};
    assert(MazeGraph_BuildFromGrid(&g, corridor, 1, 3));
    assert(MazeFeatures_Extract(&g, 0, 0, 0, 2, &f));
    assert(f.vertices == 3 && f.edges == 2 && f.dead_ends == 2);
    assert(f.junctions == 0 && f.loops == 0 && f.shortest_path == 2);
    assert(fabs(f.detour_ratio - 1.0) < 1e-12);
    assert(MazeFeatures_Extract(&g, 0, 0, 0, 0, &f));
    assert(f.shortest_path == 0 && f.detour_ratio == 1.0);

    const int square[] = {0, 0, 0, 0};
    assert(MazeGraph_BuildFromGrid(&g, square, 2, 2));
    assert(MazeFeatures_Extract(&g, 0, 0, 1, 1, &f));
    assert(f.vertices == 4 && f.edges == 4 && f.loops == 1);
    assert(f.dead_ends == 0 && fabs(f.detour_ratio - sqrt(2.0)) < 1e-12);

    const int cross[] = {1,0,1, 0,0,0, 1,0,1};
    assert(MazeGraph_BuildFromGrid(&g, cross, 3, 3));
    assert(MazeFeatures_Extract(&g, 0, 1, 2, 1, &f));
    assert(f.vertices == 5 && f.edges == 4 && f.dead_ends == 4 && f.junctions == 1);
    double vector[MAZE_FEATURE_COUNT];
    MazeFeatures_ToVector(&f, vector);
    const double expected[] = {5, 4, 4, 1, 0, 2, 1};
    for (int i = 0; i < MAZE_FEATURE_COUNT; i++) assert(vector[i] == expected[i]);
    assert(!MazeFeatures_Extract(&g, 0, 0, 2, 1, &f));
    const int disconnected[] = {0,1,0};
    assert(MazeGraph_BuildFromGrid(&g, disconnected, 1, 3));
    assert(!MazeFeatures_Extract(&g, 0, 0, 0, 2, &f));
    /* Even reachable endpoints must not hide disconnected components. */
    assert(!MazeFeatures_Extract(&g, 0, 0, 0, 0, &f));
    assert(!MazeFeatures_Extract(NULL, 0, 0, 0, 0, &f));
    MazeGraph_Free(&g);
    puts("Feature tests PASS");
    return 0;
}
