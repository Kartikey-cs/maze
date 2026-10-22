#ifndef MAZE_AUTH_H
#define MAZE_AUTH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAZE_AUTH_MAX_LEN 64

/* Both return 1 on success, 0 on failure, and write a short message (at most size bytes). */
int MazeAuth_Signup(const char *username, const char *password,
                    char *message, size_t size);
int MazeAuth_Login(const char *username, const char *password,
                   char *message, size_t size);

/* SHA-256 of data as 64 hex digits plus a zero (hex needs 65 bytes). Public for the tests. */
void MazeAuth_Sha256Hex(const unsigned char *data, size_t length, char hex[65]);

#ifdef __cplusplus
}
#endif

#endif
