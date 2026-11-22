/* maze_generator.h - makes the maze, and our random numbers (maze_generator.c) */
#ifndef MAZE_GENERATOR_H
#define MAZE_GENERATOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_ROWS 32
#define MAX_COLS 32
#define MAX_CELLS (MAX_ROWS * MAX_COLS)

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

/* random numbers: the same seed gives the same numbers on every computer */
uint32_t next_random(uint32_t *state);

#ifdef __cplusplus
}
#endif

#endif
