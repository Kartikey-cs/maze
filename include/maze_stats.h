#ifndef MAZE_STATS_H
#define MAZE_STATS_H

#include "maze_auth.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAZE_RATING_START 1000
#define MAZE_LEADERBOARD_MAX 10

typedef struct {
    char username[MAZE_AUTH_MAX_LEN];
    double rating;
    int games;
    int wins;
} MazeLeaderboardEntry;

/* Chance (0 to 1) that a player beats a maze of the given rating. */
double MazeRating_Expected(double player, double maze);

/* New rating after one game. Beating a maze you were expected to beat moves it very little. */
double MazeRating_Update(double player, double maze, int won);

/* How hard each preset counts as: 0 easy, 1 medium, 2 hard. */
double MazeRating_ForDifficulty(int difficulty);

/* Each function returns 1 on success and 0 on failure. */

int MazeStats_GetRating(const char *username, double *rating);

/* Saves one finished game and updates the player's rating in one step.
 * *new_rating receives the rating after the game. */
int MazeStats_RecordPlay(const char *username, int difficulty, int rows, int cols,
                         int won, int moves, int stamina_left, double *new_rating);

/* Fills `entries` with the best players, highest rating first. Returns how many
 * were written (0 to max), or -1 on failure. */
int MazeStats_Leaderboard(MazeLeaderboardEntry *entries, int max);

/* Writes every saved game to a CSV file. Returns the number of games written,
 * or -1 on failure. */
int MazeStats_ExportCsv(const char *path);

#ifdef __cplusplus
}
#endif

#endif
