#ifndef MAZE_DB_H
#define MAZE_DB_H

#include "sqlite3.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opens maze_game.db in the current folder and creates the tables. Returns NULL on failure. */
sqlite3 *MazeDb_Open(void);

#ifdef __cplusplus
}
#endif

#endif
