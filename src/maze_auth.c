#include "maze_auth.h"

#include "sqlite3.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAZE_AUTH_DB "maze_game.db"

/* FNV-1a, written as a 16 digit hex string. Good enough for a course project,
 * but it is not a secure way to store passwords (no salt, fast to guess). */
static void HashPassword(const char *password, char *hex)
{
    uint64_t hash = 14695981039346656037ULL;
    for (; *password; password++)
    {
        hash ^= (unsigned char)(*password);
        hash *= 1099511628211ULL;
    }
    sprintf(hex, "%016llx", (unsigned long long)hash);
}

static void SetMessage(char *message, size_t size, const char *text)
{
    snprintf(message, size, "%s", text);
}

/* Opens the database and makes sure the users table exists. */
static sqlite3 *OpenDatabase(void)
{
    sqlite3 *db = NULL;
    if (sqlite3_open(MAZE_AUTH_DB, &db) != SQLITE_OK)
    {
        sqlite3_close(db);
        return NULL;
    }
    sqlite3_exec(db,
                 "CREATE TABLE IF NOT EXISTS users("
                 "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                 "username TEXT UNIQUE, password TEXT);",
                 NULL, NULL, NULL);
    return db;
}

int MazeAuth_Signup(const char *username, const char *password,
                    char *message, size_t size)
{
    char hex[32];
    sqlite3 *db;
    sqlite3_stmt *statement = NULL;
    int success = 0;

    if (username[0] == '\0' || password[0] == '\0')
    {
        SetMessage(message, size, "Username and password can't be empty.");
        return 0;
    }

    db = OpenDatabase();
    if (!db)
    {
        SetMessage(message, size, "Could not open the database.");
        return 0;
    }

    HashPassword(password, hex);

    if (sqlite3_prepare_v2(db, "INSERT INTO users (username, password) VALUES (?, ?);",
                           -1, &statement, NULL) != SQLITE_OK)
    {
        SetMessage(message, size, "Database query error.");
    }
    else
    {
        sqlite3_bind_text(statement, 1, username, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 2, hex, -1, SQLITE_STATIC);

        if (sqlite3_step(statement) == SQLITE_DONE)
        {
            SetMessage(message, size, "Account created. You can log in now.");
            success = 1;
        }
        else
        {
            SetMessage(message, size, "That username already exists.");
        }
    }

    sqlite3_finalize(statement);
    sqlite3_close(db);
    return success;
}

int MazeAuth_Login(const char *username, const char *password,
                   char *message, size_t size)
{
    char hex[32];
    sqlite3 *db;
    sqlite3_stmt *statement = NULL;
    int success = 0;

    db = OpenDatabase();
    if (!db)
    {
        SetMessage(message, size, "Could not open the database.");
        return 0;
    }

    HashPassword(password, hex);

    if (sqlite3_prepare_v2(db, "SELECT id FROM users WHERE username = ? AND password = ?;",
                           -1, &statement, NULL) != SQLITE_OK)
    {
        SetMessage(message, size, "Database query error.");
    }
    else
    {
        sqlite3_bind_text(statement, 1, username, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 2, hex, -1, SQLITE_STATIC);

        if (sqlite3_step(statement) == SQLITE_ROW)
        {
            SetMessage(message, size, "Login successful.");
            success = 1;
        }
        else
        {
            SetMessage(message, size, "Wrong username or password.");
        }
    }

    sqlite3_finalize(statement);
    sqlite3_close(db);
    return success;
}
