#ifndef MAZE_GENERATOR_H
#define MAZE_GENERATOR_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── Difficulty levels ────────────────────────────────────────────────── */

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

/* ── Core generation API ──────────────────────────────────────────────── */

void MazeGen_GetParams(MazeDifficulty diff, MazeDifficultyParams *out);

int MazeGen_Generate(int *grid, int rows, int cols, int loop_percent,
                     unsigned int seed);

void MazeGen_ToCharGrid(const int *grid, int rows, int cols, char *out);

void MazeGen_PrintCharGrid(const char *charGrid, int rows, int cols);

#ifdef __cplusplus
}
#endif

#endif