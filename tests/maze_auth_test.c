#include "maze_auth.h"
#include "maze_db.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* The database file is created in the current folder, so the Makefile runs
 * this test from build/auth_test/ and never touches a real maze_game.db. */

static void StoredPassword(const char *username, char *out, size_t size)
{
    sqlite3 *db = MazeDb_Open();
    sqlite3_stmt *statement = NULL;
    assert(db);
    assert(sqlite3_prepare_v2(db, "SELECT password FROM users WHERE username = ?;",
                              -1, &statement, NULL) == SQLITE_OK);
    sqlite3_bind_text(statement, 1, username, -1, SQLITE_STATIC);
    assert(sqlite3_step(statement) == SQLITE_ROW);
    snprintf(out, size, "%s", (const char *)sqlite3_column_text(statement, 0));
    sqlite3_finalize(statement);
    sqlite3_close(db);
}

int main(void)
{
    char message[128];
    char hex[65];

    /* SHA-256 against the published test answers (empty, short, two blocks). */
    MazeAuth_Sha256Hex((const unsigned char *)"", 0, hex);
    assert(strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0);
    MazeAuth_Sha256Hex((const unsigned char *)"abc", 3, hex);
    assert(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);
    const char *long_text = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    MazeAuth_Sha256Hex((const unsigned char *)long_text, strlen(long_text), hex);
    assert(strcmp(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") == 0);

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

    /* Salting: same password, different stored text, and no plain password. */
    char first[128], second[128];
    assert(MazeAuth_Signup("carol", "samepw", message, sizeof message));
    assert(MazeAuth_Signup("dave", "samepw", message, sizeof message));
    StoredPassword("carol", first, sizeof first);
    StoredPassword("dave", second, sizeof second);
    assert(strcmp(first, second) != 0);
    assert(strchr(first, ':') != NULL);
    assert(strstr(first, "samepw") == NULL);

    /* Passwords that are too long are refused, not cut off. */
    char huge[200];
    memset(huge, 'x', sizeof huge - 1);
    huge[sizeof huge - 1] = '\0';
    assert(!MazeAuth_Signup("erin", huge, message, sizeof message));
    assert(!MazeAuth_Login("erin", huge, message, sizeof message));

    /* An account from the old unsalted format still logs in, then is upgraded. */
    {
        uint64_t old = 14695981039346656037ULL;
        const char *p;
        char old_hex[32];
        sqlite3 *db = MazeDb_Open();
        sqlite3_stmt *statement = NULL;
        for (p = "oldpw"; *p; p++) { old ^= (unsigned char)*p; old *= 1099511628211ULL; }
        sprintf(old_hex, "%016llx", (unsigned long long)old);
        assert(sqlite3_prepare_v2(db, "INSERT INTO users (username, password) VALUES ('frank', ?);",
                                  -1, &statement, NULL) == SQLITE_OK);
        sqlite3_bind_text(statement, 1, old_hex, -1, SQLITE_STATIC);
        assert(sqlite3_step(statement) == SQLITE_DONE);
        sqlite3_finalize(statement);
        sqlite3_close(db);
    }
    assert(!MazeAuth_Login("frank", "nope", message, sizeof message));
    assert(MazeAuth_Login("frank", "oldpw", message, sizeof message));
    StoredPassword("frank", first, sizeof first);
    assert(strchr(first, ':') != NULL);                      /* now salted */
    assert(MazeAuth_Login("frank", "oldpw", message, sizeof message));

    puts("Auth tests PASS");
    return 0;
}
