#include "maze_db.h"

#include <stddef.h>

sqlite3 *MazeDb_Open(void)
{
    sqlite3 *db = NULL;
    if (sqlite3_open("maze_game.db", &db) != SQLITE_OK)
    {
        sqlite3_close(db);
        return NULL;
    }

    sqlite3_exec(db,
                 "CREATE TABLE IF NOT EXISTS users("
                 "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                 "username TEXT UNIQUE, password TEXT);",
                 NULL, NULL, NULL);

    /* Older databases have no rating column; the error when it already exists is ignored. */
    sqlite3_exec(db, "ALTER TABLE users ADD COLUMN rating REAL DEFAULT 1000;",
                 NULL, NULL, NULL);

    /* difficulty: 0 easy, 1 medium, 2 hard */
    sqlite3_exec(db,
                 "CREATE TABLE IF NOT EXISTS plays("
                 "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                 "user_id INTEGER, "
                 "played_at TEXT DEFAULT CURRENT_TIMESTAMP, "
                 "difficulty INTEGER, rows INTEGER, cols INTEGER, "
                 "won INTEGER, moves INTEGER, stamina_left INTEGER, "
                 "rating_after REAL);",
                 NULL, NULL, NULL);

    return db;
}
