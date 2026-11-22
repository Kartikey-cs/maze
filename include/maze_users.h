/* maze_users.h - accounts, ratings and saved games (maze_users.c) */
#ifndef MAZE_USERS_H
#define MAZE_USERS_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LEN 64              /* longest username / password + 1 */
#define START_RATING 1000
#define LEADERBOARD_SIZE 10

/* both return 1 if it worked, and write a short message for the screen */
int signup_user(const char *username, const char *password, char *message, int size);
int login_user(const char *username, const char *password, char *message, int size);

double expected_score(double player, double maze);           /* chance (0 to 1) to win */
double new_rating(double player, double maze, int won);
double difficulty_rating(int difficulty);                    /* 0 easy, 1 medium, 2 hard */

typedef struct {
    char username[MAX_LEN];
    double rating;
    int games;
    int wins;
} LeaderboardRow;

int get_rating(const char *username, double *rating);         /* 1 if found */
/* saves one finished game and updates the rating. returns 1 if it worked */
int save_game(const char *username, int difficulty, int rows, int cols,
              int won, int moves, int stamina_left, double *rating_after);
int get_leaderboard(LeaderboardRow *rows, int max);           /* how many rows, or -1 */
int export_csv(const char *path);                             /* how many games, or -1 */

#ifdef __cplusplus
}
#endif

#endif
