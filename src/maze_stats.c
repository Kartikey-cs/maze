#include "maze_stats.h"
#include "maze_db.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define RATING_K 32.0   /* how far one game can move a rating */

/* ---------------------------------------------------------------------------
 * Rating maths
 * ------------------------------------------------------------------------- */

double MazeRating_Expected(double player, double maze)
{
    return 1.0 / (1.0 + pow(10.0, (maze - player) / 400.0));
}

double MazeRating_Update(double player, double maze, int won)
{
    double expected = MazeRating_Expected(player, maze);
    return player + RATING_K * ((won ? 1.0 : 0.0) - expected);
}

double MazeRating_ForDifficulty(int difficulty)
{
    /* Fixed numbers for now. Later these can come from the difficulty model. */
    if (difficulty <= 0) return 800;
    if (difficulty == 1) return 1000;
    return 1200;
}

/* ---------------------------------------------------------------------------
 * Database
 * ------------------------------------------------------------------------- */

int MazeStats_GetRating(const char *username, double *rating)
{
    sqlite3 *db = MazeDb_Open();
    sqlite3_stmt *statement = NULL;
    int found = 0;

    if (!db) return 0;

    if (sqlite3_prepare_v2(db, "SELECT rating FROM users WHERE username = ?;",
                           -1, &statement, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(statement, 1, username, -1, SQLITE_STATIC);
        if (sqlite3_step(statement) == SQLITE_ROW)
        {
            *rating = sqlite3_column_double(statement, 0);
            found = 1;
        }
    }

    sqlite3_finalize(statement);
    sqlite3_close(db);
    return found;
}

int MazeStats_RecordPlay(const char *username, int difficulty, int rows, int cols,
                         int won, int moves, int stamina_left, double *new_rating)
{
    sqlite3 *db = MazeDb_Open();
    sqlite3_stmt *statement = NULL;
    sqlite3_int64 user_id = 0;
    double rating = 0;
    int ok = 0;

    if (!db) return 0;

    /* The rating update and the saved game must happen together or not at all. */
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);

    if (sqlite3_prepare_v2(db, "SELECT id, rating FROM users WHERE username = ?;",
                           -1, &statement, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(statement, 1, username, -1, SQLITE_STATIC);
        if (sqlite3_step(statement) == SQLITE_ROW)
        {
            user_id = sqlite3_column_int64(statement, 0);
            rating = sqlite3_column_double(statement, 1);
            ok = 1;
        }
    }
    sqlite3_finalize(statement);
    statement = NULL;

    if (ok)
    {
        rating = MazeRating_Update(rating, MazeRating_ForDifficulty(difficulty), won);

        ok = sqlite3_prepare_v2(db,
                 "INSERT INTO plays (user_id, difficulty, rows, cols, won, moves, "
                 "stamina_left, rating_after) VALUES (?, ?, ?, ?, ?, ?, ?, ?);",
                 -1, &statement, NULL) == SQLITE_OK;
    }
    if (ok)
    {
        sqlite3_bind_int64(statement, 1, user_id);
        sqlite3_bind_int(statement, 2, difficulty);
        sqlite3_bind_int(statement, 3, rows);
        sqlite3_bind_int(statement, 4, cols);
        sqlite3_bind_int(statement, 5, won ? 1 : 0);
        sqlite3_bind_int(statement, 6, moves);
        sqlite3_bind_int(statement, 7, stamina_left);
        sqlite3_bind_double(statement, 8, rating);
        ok = sqlite3_step(statement) == SQLITE_DONE;
    }
    sqlite3_finalize(statement);
    statement = NULL;

    if (ok)
    {
        ok = sqlite3_prepare_v2(db, "UPDATE users SET rating = ? WHERE id = ?;",
                                -1, &statement, NULL) == SQLITE_OK;
    }
    if (ok)
    {
        sqlite3_bind_double(statement, 1, rating);
        sqlite3_bind_int64(statement, 2, user_id);
        ok = sqlite3_step(statement) == SQLITE_DONE;
    }
    sqlite3_finalize(statement);

    sqlite3_exec(db, ok ? "COMMIT;" : "ROLLBACK;", NULL, NULL, NULL);
    sqlite3_close(db);

    if (ok && new_rating) *new_rating = rating;
    return ok;
}

int MazeStats_Leaderboard(MazeLeaderboardEntry *entries, int max)
{
    sqlite3 *db = MazeDb_Open();
    sqlite3_stmt *statement = NULL;
    int count = 0;

    if (!db) return -1;

    /* One row per player: rating, how many games, how many wins. */
    if (sqlite3_prepare_v2(db,
            "SELECT u.username, u.rating, COUNT(p.id), COALESCE(SUM(p.won), 0) "
            "FROM users u LEFT JOIN plays p ON p.user_id = u.id "
            "GROUP BY u.id ORDER BY u.rating DESC, u.username LIMIT ?;",
            -1, &statement, NULL) != SQLITE_OK)
    {
        sqlite3_close(db);
        return -1;
    }

    sqlite3_bind_int(statement, 1, max);
    while (count < max && sqlite3_step(statement) == SQLITE_ROW)
    {
        snprintf(entries[count].username, sizeof entries[count].username, "%s",
                 (const char *)sqlite3_column_text(statement, 0));
        entries[count].rating = sqlite3_column_double(statement, 1);
        entries[count].games = sqlite3_column_int(statement, 2);
        entries[count].wins = sqlite3_column_int(statement, 3);
        count++;
    }

    sqlite3_finalize(statement);
    sqlite3_close(db);
    return count;
}

/* Writes text as one CSV field. It is quoted, and a quote inside it is doubled,
 * so names containing commas or quotes do not break the columns. */
static void WriteCsvText(FILE *file, const char *text)
{
    fputc('"', file);
    for (; *text; text++)
    {
        if (*text == '"') fputc('"', file);
        fputc(*text, file);
    }
    fputc('"', file);
}

int MazeStats_ExportCsv(const char *path)
{
    sqlite3 *db = MazeDb_Open();
    sqlite3_stmt *statement = NULL;
    FILE *file;
    int count = 0;

    if (!db) return -1;

    file = fopen(path, "w");
    if (!file)
    {
        sqlite3_close(db);
        return -1;
    }

    if (sqlite3_prepare_v2(db,
            "SELECT u.username, p.played_at, p.difficulty, p.rows, p.cols, p.won, "
            "p.moves, p.stamina_left, p.rating_after "
            "FROM plays p JOIN users u ON u.id = p.user_id ORDER BY p.id;",
            -1, &statement, NULL) != SQLITE_OK)
    {
        fclose(file);
        sqlite3_close(db);
        return -1;
    }

    fputs("username,played_at,difficulty,rows,cols,won,moves,stamina_left,rating_after\n", file);
    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        WriteCsvText(file, (const char *)sqlite3_column_text(statement, 0));
        fputc(',', file);
        WriteCsvText(file, (const char *)sqlite3_column_text(statement, 1));
        fprintf(file, ",%d,%d,%d,%d,%d,%d,%.2f\n",
                sqlite3_column_int(statement, 2), sqlite3_column_int(statement, 3),
                sqlite3_column_int(statement, 4), sqlite3_column_int(statement, 5),
                sqlite3_column_int(statement, 6), sqlite3_column_int(statement, 7),
                sqlite3_column_double(statement, 8));
        count++;
    }

    sqlite3_finalize(statement);
    sqlite3_close(db);
    if (fclose(file) != 0) return -1;
    return count;
}
