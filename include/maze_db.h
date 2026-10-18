#ifndef MAZE_DB_H
#define MAZE_DB_H

#include "sqlite3.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opens maze_game.db in the current folder and makes sure every table exists
 * (users and plays). Returns NULL on failure. Close it with sqlite3_close(). */
sqlite3 *MazeDb_Open(void);

#ifdef __cplusplus
}
#endif

#endif
