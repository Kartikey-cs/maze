#ifndef MAZE_GENERATOR_H
#define MAZE_GENERATOR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MAZE_DIFFICULTY_EASY,
    MAZE_DIFFICULTY_MEDIUM,
    MAZE_DIFFICULTY_HARD
} MazeDifficulty;

typedef struct {
    int rows;
    int cols;
    int loop_percent;
    int fruit_count;
} MazeDifficultyParams;

void MazeGen_GetParams(MazeDifficulty diff, MazeDifficultyParams *out);
int MazeGen_Generate(int *grid, int rows, int cols, int loop_percent, unsigned int seed);

#ifdef __cplusplus
}
#endif

#endif
