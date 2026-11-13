/* maze_users.c - accounts (sign up / log in), ratings, leaderboard and saved games.
   Everything is stored with SQLite in maze_game.db */
#include "maze.h"
#include <math.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

/* ================= database ================= */

/* opens maze_game.db in the current folder and makes the tables. NULL if it fails */
static sqlite3 *open_db(void)
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

    /* older databases have no rating column. if it is already there this
       gives an error, which we ignore */
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

/* ================= SHA-256 ================= */

/* constants from the SHA-256 standard (FIPS 180-4) */
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/* rotate right */
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

/* mixes one 64 byte block into the hash state h */
static void sha256_block(uint32_t h[8], const unsigned char *p)
{
    uint32_t w[64];
    int i;

    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[4 * i] << 24) | ((uint32_t)p[4 * i + 1] << 16) |
               ((uint32_t)p[4 * i + 2] << 8) | (uint32_t)p[4 * i + 3];
    for (i = 16; i < 64; i++)
    {
        uint32_t s0 = ROTR(w[i - 15], 7) ^ ROTR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROTR(w[i - 2], 17) ^ ROTR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
    for (i = 0; i < 64; i++)
    {
        uint32_t s1 = ROTR(e, 6) ^ ROTR(e, 11) ^ ROTR(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = hh + s1 + ch + K[i] + w[i];
        uint32_t s0 = ROTR(a, 2) ^ ROTR(a, 13) ^ ROTR(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = s0 + maj;
        hh = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

static void sha256(const unsigned char *data, size_t length, unsigned char out[32])
{
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    unsigned char block[128];
    size_t whole = length / 64;          /* full 64 byte blocks */
    size_t rest = length - whole * 64;   /* bytes left over */
    size_t padded = 128;
    size_t i;

    if (rest < 56)
        padded = 64;

    for (i = 0; i < whole; i++)
        sha256_block(h, data + i * 64);

    /* padding: the rest of the data, a 1 bit, zeros, then the length in bits */
    memcpy(block, data + whole * 64, rest);
    block[rest] = 0x80;
    memset(block + rest + 1, 0, padded - rest - 1);
    uint64_t bits = (uint64_t)length * 8;
    for (i = 0; i < 8; i++)
        block[padded - 1 - i] = (unsigned char)(bits >> (8 * i));

    sha256_block(h, block);
    if (padded == 128)
        sha256_block(h, block + 64);

    for (i = 0; i < 8; i++)
    {
        out[4 * i] = (unsigned char)(h[i] >> 24);
        out[4 * i + 1] = (unsigned char)(h[i] >> 16);
        out[4 * i + 2] = (unsigned char)(h[i] >> 8);
        out[4 * i + 3] = (unsigned char)h[i];
    }
}

static void to_hex(const unsigned char *bytes, int count, char *hex)
{
    int i;
    for (i = 0; i < count; i++)
        snprintf(hex + 2 * i, 3, "%02x", bytes[i]);
}

/* ================= passwords ================= */

/* A password is saved as "<32 hex salt>:<64 hex hash>".
   The salt is random for every account, and the hash is SHA-256 done
   10000 times, so guessing passwords is slow. */
#define SALT_BYTES 16
#define HASH_ROUNDS 10000
#define STORED_LEN (2 * SALT_BYTES + 1 + 64 + 1)

static void hash_password(const unsigned char salt[SALT_BYTES], const char *password, char hex[65])
{
    unsigned char buffer[32 + SALT_BYTES + MAX_LEN];
    unsigned char digest[32] = {0};
    size_t length = strlen(password);
    int round;

    for (round = 0; round < HASH_ROUNDS; round++)
    {
        /* digest = SHA256(previous digest + salt + password) */
        memcpy(buffer, digest, 32);
        memcpy(buffer + 32, salt, SALT_BYTES);
        memcpy(buffer + 32 + SALT_BYTES, password, length);
        sha256(buffer, 32 + SALT_BYTES + length, digest);
    }
    to_hex(digest, 32, hex);
}

/* makes a new random salt and the text that goes in the database */
static void make_stored_password(const char *password, char stored[STORED_LEN])
{
    unsigned char salt[SALT_BYTES];
    char hash[65];

    sqlite3_randomness(SALT_BYTES, salt);
    hash_password(salt, password, hash);
    to_hex(salt, SALT_BYTES, stored);
    stored[2 * SALT_BYTES] = ':';
    memcpy(stored + 2 * SALT_BYTES + 1, hash, 65);
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

/* compares the whole text even after a difference, so the time taken
   doesn't tell an attacker how much was right */
static int same_text(const char *a, const char *b)
{
    unsigned char difference = 0;
    size_t length = strlen(a);
    size_t i;

    if (length != strlen(b))
        return 0;
    for (i = 0; i < length; i++)
        difference |= (unsigned char)(a[i] ^ b[i]);
    return difference == 0;
}

/* old accounts saved an unsalted 64 bit FNV hash. they can still log in,
   and get the new format when they do */
static void legacy_hash(const char *password, char hex[32])
{
    unsigned long long hash = 14695981039346656037ULL;
    int i;
    for (i = 0; password[i] != '\0'; i++)
    {
        hash ^= (unsigned char)password[i];
        hash *= 1099511628211ULL;
    }
    snprintf(hex, 32, "%016llx", hash);
}

/* returns 1 if the password is right. *legacy = 1 if it was the old format */
static int check_password(const char *password, const char *stored, int *legacy)
{
    unsigned char salt[SALT_BYTES];
    char hash[65];
    int i;

    *legacy = 0;
    if (strchr(stored, ':') == NULL)
        *legacy = 1;

    if (*legacy)
    {
        char hex[32];
        legacy_hash(password, hex);
        return same_text(hex, stored);
    }

    if (strlen(stored) != STORED_LEN - 1 || stored[2 * SALT_BYTES] != ':')
        return 0;

    /* read the salt back from hex */
    for (i = 0; i < SALT_BYTES; i++)
    {
        int high = hex_value(stored[2 * i]);
        int low = hex_value(stored[2 * i + 1]);
        if (high < 0 || low < 0)
            return 0;
        salt[i] = (unsigned char)(high * 16 + low);
    }
    hash_password(salt, password, hash);
    return same_text(hash, stored + 2 * SALT_BYTES + 1);
}

/* ================= sign up / log in ================= */

int signup_user(const char *username, const char *password, char *message, int size)
{
    char stored[STORED_LEN];
    sqlite3 *db;
    sqlite3_stmt *stmt = NULL;
    int success = 0;

    if (username[0] == '\0' || password[0] == '\0')
    {
        snprintf(message, size, "Username and password can't be empty.");
        return 0;
    }
    if (strlen(username) >= MAX_LEN || strlen(password) >= MAX_LEN)
    {
        snprintf(message, size, "Username or password is too long.");
        return 0;
    }

    db = open_db();
    if (db == NULL)
    {
        snprintf(message, size, "Could not open the database.");
        return 0;
    }

    make_stored_password(password, stored);

    /* the ? are filled in with bind_text, which stops SQL injection */
    if (sqlite3_prepare_v2(db, "INSERT INTO users (username, password) VALUES (?, ?);",
                           -1, &stmt, NULL) != SQLITE_OK)
    {
        snprintf(message, size, "Database query error.");
    }
    else
    {
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, stored, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            snprintf(message, size, "Account created. You can log in now.");
            success = 1;
        }
        else
        {
            snprintf(message, size, "That username already exists.");
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return success;
}

int login_user(const char *username, const char *password, char *message, int size)
{
    char stored[128] = "";
    sqlite3 *db;
    sqlite3_stmt *stmt = NULL;
    int found = 0;
    int success = 0;
    int legacy = 0;

    if (strlen(password) >= MAX_LEN)
    {
        snprintf(message, size, "Wrong username or password.");
        return 0;
    }

    db = open_db();
    if (db == NULL)
    {
        snprintf(message, size, "Could not open the database.");
        return 0;
    }

    if (sqlite3_prepare_v2(db, "SELECT password FROM users WHERE username = ?;",
                           -1, &stmt, NULL) != SQLITE_OK)
    {
        snprintf(message, size, "Database query error.");
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        snprintf(stored, sizeof(stored), "%s", (const char *)sqlite3_column_text(stmt, 0));
        found = 1;
    }
    sqlite3_finalize(stmt);

    if (found && check_password(password, stored, &legacy))
    {
        success = 1;
        snprintf(message, size, "Login successful.");

        /* save old accounts again in the new, safer format */
        if (legacy)
        {
            char upgraded[STORED_LEN];
            make_stored_password(password, upgraded);
            if (sqlite3_prepare_v2(db, "UPDATE users SET password = ? WHERE username = ?;",
                                   -1, &stmt, NULL) == SQLITE_OK)
            {
                sqlite3_bind_text(stmt, 1, upgraded, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, username, -1, SQLITE_STATIC);
                sqlite3_step(stmt);
            }
            sqlite3_finalize(stmt);
        }
    }
    else
    {
        snprintf(message, size, "Wrong username or password.");
    }

    sqlite3_close(db);
    return success;
}

/* ================= ratings (Elo) ================= */

#define RATING_K 32.0   /* how much one game can change a rating */

double expected_score(double player, double maze)
{
    return 1.0 / (1.0 + pow(10.0, (maze - player) / 400.0));
}

double new_rating(double player, double maze, int won)
{
    double result = 0.0;
    if (won)
        result = 1.0;
    return player + RATING_K * (result - expected_score(player, maze));
}

double difficulty_rating(int difficulty)
{
    /* fixed numbers for now. later these could come from the difficulty model */
    if (difficulty <= 0)
        return 800;
    if (difficulty == 1)
        return 1000;
    return 1200;
}

int get_rating(const char *username, double *rating)
{
    sqlite3 *db = open_db();
    sqlite3_stmt *stmt = NULL;
    int found = 0;

    if (db == NULL)
        return 0;

    if (sqlite3_prepare_v2(db, "SELECT rating FROM users WHERE username = ?;",
                           -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            *rating = sqlite3_column_double(stmt, 0);
            found = 1;
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return found;
}

int save_game(const char *username, int difficulty, int rows, int cols,
              int won, int moves, int stamina_left, double *rating_after)
{
    sqlite3 *db = open_db();
    sqlite3_stmt *stmt = NULL;
    sqlite3_int64 user_id = 0;
    double rating = 0;
    int ok = 0;

    if (db == NULL)
        return 0;

    /* saving the game and changing the rating must both happen, or neither */
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);

    /* step 1: find the player */
    if (sqlite3_prepare_v2(db, "SELECT id, rating FROM users WHERE username = ?;",
                           -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            user_id = sqlite3_column_int64(stmt, 0);
            rating = sqlite3_column_double(stmt, 1);
            ok = 1;
        }
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    /* step 2: save the game with the new rating */
    if (ok)
    {
        rating = new_rating(rating, difficulty_rating(difficulty), won);
        ok = sqlite3_prepare_v2(db,
                 "INSERT INTO plays (user_id, difficulty, rows, cols, won, moves, "
                 "stamina_left, rating_after) VALUES (?, ?, ?, ?, ?, ?, ?, ?);",
                 -1, &stmt, NULL) == SQLITE_OK;
    }
    if (ok)
    {
        sqlite3_bind_int64(stmt, 1, user_id);
        sqlite3_bind_int(stmt, 2, difficulty);
        sqlite3_bind_int(stmt, 3, rows);
        sqlite3_bind_int(stmt, 4, cols);
        sqlite3_bind_int(stmt, 5, won ? 1 : 0);
        sqlite3_bind_int(stmt, 6, moves);
        sqlite3_bind_int(stmt, 7, stamina_left);
        sqlite3_bind_double(stmt, 8, rating);
        ok = sqlite3_step(stmt) == SQLITE_DONE;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    /* step 3: update the player's rating */
    if (ok)
    {
        ok = sqlite3_prepare_v2(db, "UPDATE users SET rating = ? WHERE id = ?;",
                                -1, &stmt, NULL) == SQLITE_OK;
    }
    if (ok)
    {
        sqlite3_bind_double(stmt, 1, rating);
        sqlite3_bind_int64(stmt, 2, user_id);
        ok = sqlite3_step(stmt) == SQLITE_DONE;
    }
    sqlite3_finalize(stmt);

    if (ok)
        sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    else
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
    sqlite3_close(db);

    if (ok && rating_after != NULL)
        *rating_after = rating;
    return ok;
}

int get_leaderboard(LeaderboardRow *rows, int max)
{
    sqlite3 *db = open_db();
    sqlite3_stmt *stmt = NULL;
    int count = 0;

    if (db == NULL)
        return -1;

    /* one row per player: rating, number of games, number of wins */
    if (sqlite3_prepare_v2(db,
            "SELECT u.username, u.rating, COUNT(p.id), COALESCE(SUM(p.won), 0) "
            "FROM users u LEFT JOIN plays p ON p.user_id = u.id "
            "GROUP BY u.id ORDER BY u.rating DESC, u.username LIMIT ?;",
            -1, &stmt, NULL) != SQLITE_OK)
    {
        sqlite3_close(db);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, max);
    while (count < max && sqlite3_step(stmt) == SQLITE_ROW)
    {
        snprintf(rows[count].username, sizeof(rows[count].username), "%s",
                 (const char *)sqlite3_column_text(stmt, 0));
        rows[count].rating = sqlite3_column_double(stmt, 1);
        rows[count].games = sqlite3_column_int(stmt, 2);
        rows[count].wins = sqlite3_column_int(stmt, 3);
        count++;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return count;
}

/* writes text as one CSV field: in quotes, with any " written twice */
static void write_csv_text(FILE *file, const char *text)
{
    int i;
    fputc('"', file);
    for (i = 0; text[i] != '\0'; i++)
    {
        if (text[i] == '"')
            fputc('"', file);
        fputc(text[i], file);
    }
    fputc('"', file);
}

int export_csv(const char *path)
{
    sqlite3 *db = open_db();
    sqlite3_stmt *stmt = NULL;
    FILE *file;
    int count = 0;

    if (db == NULL)
        return -1;

    file = fopen(path, "w");
    if (file == NULL)
    {
        sqlite3_close(db);
        return -1;
    }

    if (sqlite3_prepare_v2(db,
            "SELECT u.username, p.played_at, p.difficulty, p.rows, p.cols, p.won, "
            "p.moves, p.stamina_left, p.rating_after "
            "FROM plays p JOIN users u ON u.id = p.user_id ORDER BY p.id;",
            -1, &stmt, NULL) != SQLITE_OK)
    {
        fclose(file);
        sqlite3_close(db);
        return -1;
    }

    fprintf(file, "username,played_at,difficulty,rows,cols,won,moves,stamina_left,rating_after\n");
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        write_csv_text(file, (const char *)sqlite3_column_text(stmt, 0));
        fprintf(file, ",");
        write_csv_text(file, (const char *)sqlite3_column_text(stmt, 1));
        fprintf(file, ",%d,%d,%d,%d,%d,%d,%.2f\n",
                sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 4), sqlite3_column_int(stmt, 5),
                sqlite3_column_int(stmt, 6), sqlite3_column_int(stmt, 7),
                sqlite3_column_double(stmt, 8));
        count++;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    if (fclose(file) != 0)
        return -1;
    return count;
}
