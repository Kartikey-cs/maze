#include "maze_auth.h"
#include "maze_stats.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Run from build/stats_test/ so the database and CSV are made in a scratch folder. */

static void TestRatingMaths(void)
{
    /* Equal ratings: a coin flip, so a win gains 16 and a loss loses 16. */
    assert(fabs(MazeRating_Expected(1000, 1000) - 0.5) < 1e-12);
    assert(fabs(MazeRating_Update(1000, 1000, 1) - 1016) < 1e-9);
    assert(fabs(MazeRating_Update(1000, 1000, 0) - 984) < 1e-9);

    /* Beating a much easier maze barely counts; losing to it hurts a lot. */
    assert(MazeRating_Update(1400, 800, 1) - 1400 < 1.0);
    assert(1400 - MazeRating_Update(1400, 800, 0) > 30.0);

    /* A win always raises the rating and a loss always lowers it. */
    for (int player = 600; player <= 1600; player += 100)
        for (int maze = 800; maze <= 1200; maze += 200)
        {
            assert(MazeRating_Update(player, maze, 1) > player);
            assert(MazeRating_Update(player, maze, 0) < player);
        }

    assert(MazeRating_ForDifficulty(0) < MazeRating_ForDifficulty(1));
    assert(MazeRating_ForDifficulty(1) < MazeRating_ForDifficulty(2));
}

int main(void)
{
    char message[128];
    double rating = 0;

    TestRatingMaths();

    assert(MazeAuth_Signup("alice", "pw", message, sizeof message));
    assert(MazeAuth_Signup("bob", "pw", message, sizeof message));
    assert(MazeAuth_Signup("a,\"b\"", "pw", message, sizeof message));   /* awkward name */

    /* New accounts start at the starting rating. */
    assert(MazeStats_GetRating("alice", &rating));
    assert(rating == MAZE_RATING_START);
    assert(!MazeStats_GetRating("nobody", &rating));

    /* Saving a game updates the rating and returns the new one. */
    double after = 0;
    assert(MazeStats_RecordPlay("alice", 1, 21, 21, 1, 70, 52, &after));
    assert(fabs(after - 1016) < 1e-9);
    assert(MazeStats_GetRating("alice", &rating) && fabs(rating - after) < 1e-9);

    assert(MazeStats_RecordPlay("alice", 2, 31, 31, 0, 100, 0, &after));
    assert(after < 1016);

    /* An unknown player must not create a game row. */
    assert(!MazeStats_RecordPlay("nobody", 0, 15, 15, 1, 30, 70, &after));

    assert(MazeStats_RecordPlay("bob", 0, 15, 15, 1, 40, 60, NULL));
    assert(MazeStats_RecordPlay("a,\"b\"", 0, 15, 15, 1, 41, 59, NULL));

    /* Leaderboard: highest rating first, with game and win counts. */
    MazeLeaderboardEntry board[MAZE_LEADERBOARD_MAX];
    int count = MazeStats_Leaderboard(board, MAZE_LEADERBOARD_MAX);
    assert(count == 3);
    for (int i = 1; i < count; i++)
        assert(board[i - 1].rating >= board[i].rating);
    for (int i = 0; i < count; i++)
        if (strcmp(board[i].username, "alice") == 0)
        {
            assert(board[i].games == 2 && board[i].wins == 1);
        }

    /* The list is cut off at max. */
    assert(MazeStats_Leaderboard(board, 2) == 2);

    /* CSV: header plus one line per saved game, and the awkward name is quoted. */
    int rows = MazeStats_ExportCsv("play_log.csv");
    assert(rows == 4);
    FILE *file = fopen("play_log.csv", "r");
    assert(file);
    char line[256];
    int lines = 0;
    int found_quoted = 0;
    while (fgets(line, sizeof line, file))
    {
        lines++;
        if (strncmp(line, "\"a,\"\"b\"\"\",", 10) == 0) found_quoted = 1;
    }
    fclose(file);
    assert(lines == 5);
    assert(found_quoted);

    puts("Stats tests PASS");
    return 0;
}
