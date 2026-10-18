#include "maze_auth.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* The database file is created in the current folder, so the Makefile runs
 * this test from build/auth_test/ and never touches a real maze_game.db. */
int main(void)
{
    char message[128];

    /* Login before any account exists must fail cleanly, not crash. */
    assert(!MazeAuth_Login("nobody", "pw", message, sizeof message));

    assert(!MazeAuth_Signup("", "pw", message, sizeof message));
    assert(!MazeAuth_Signup("alice", "", message, sizeof message));

    assert(MazeAuth_Signup("alice", "secret", message, sizeof message));
    assert(!MazeAuth_Signup("alice", "other", message, sizeof message));
    assert(strstr(message, "exists") != NULL);

    assert(MazeAuth_Login("alice", "secret", message, sizeof message));
    assert(!MazeAuth_Login("alice", "wrong", message, sizeof message));
    assert(!MazeAuth_Login("bob", "secret", message, sizeof message));

    /* Quotes must be treated as plain text, not as SQL. */
    assert(!MazeAuth_Login("alice' OR '1'='1", "x", message, sizeof message));

    /* A short buffer must still end with a terminating zero. */
    char tiny[6];
    assert(!MazeAuth_Login("alice", "wrong", tiny, sizeof tiny));
    assert(strlen(tiny) < sizeof tiny);

    puts("Auth tests PASS");
    return 0;
}
