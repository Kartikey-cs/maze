#ifndef MAZE_RNG_H
#define MAZE_RNG_H
#include <stdint.h>
/* Nonzero state, explicit 32-bit arithmetic for reproducible experiments. */
static inline uint32_t MazeRng_Next(uint32_t *state)
{
    uint32_t x = *state ? *state : 1;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}
#endif
