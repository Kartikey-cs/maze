#include "maze_auth.h"
#include "maze_db.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * SHA-256
 * ------------------------------------------------------------------------- */

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

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

/* Mixes one 64 byte block into the running state h. */
static void Sha256Block(uint32_t h[8], const unsigned char *p)
{
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[4 * i] << 24) | ((uint32_t)p[4 * i + 1] << 16) |
               ((uint32_t)p[4 * i + 2] << 8) | (uint32_t)p[4 * i + 3];
    for (int i = 16; i < 64; i++)
    {
        uint32_t s0 = ROTR(w[i - 15], 7) ^ ROTR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROTR(w[i - 2], 17) ^ ROTR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++)
    {
        uint32_t s1 = ROTR(e, 6) ^ ROTR(e, 11) ^ ROTR(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = hh + s1 + ch + K[i] + w[i];
        uint32_t s0 = ROTR(a, 2) ^ ROTR(a, 13) ^ ROTR(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = s0 + maj;
        hh = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

static void Sha256(const unsigned char *data, size_t length, unsigned char out[32])
{
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    unsigned char block[128];   /* the padded tail: one or two blocks */
    size_t whole = length / 64;
    size_t rest = length - whole * 64;
    size_t padded = (rest < 56) ? 64 : 128;

    for (size_t i = 0; i < whole; i++)
        Sha256Block(h, data + i * 64);

    /* Padding: the data, a 1 bit, zeros, then the length in bits. */
    memcpy(block, data + whole * 64, rest);
    block[rest] = 0x80;
    memset(block + rest + 1, 0, padded - rest - 1);
    uint64_t bits = (uint64_t)length * 8;
    for (int i = 0; i < 8; i++)
        block[padded - 1 - i] = (unsigned char)(bits >> (8 * i));

    Sha256Block(h, block);
    if (padded == 128)
        Sha256Block(h, block + 64);

    for (int i = 0; i < 8; i++)
    {
        out[4 * i] = (unsigned char)(h[i] >> 24);
        out[4 * i + 1] = (unsigned char)(h[i] >> 16);
        out[4 * i + 2] = (unsigned char)(h[i] >> 8);
        out[4 * i + 3] = (unsigned char)h[i];
    }
}

static void ToHex(const unsigned char *bytes, size_t count, char *hex)
{
    for (size_t i = 0; i < count; i++)
        sprintf(hex + 2 * i, "%02x", bytes[i]);
}

void MazeAuth_Sha256Hex(const unsigned char *data, size_t length, char hex[65])
{
    unsigned char digest[32];
    Sha256(data, length, digest);
    ToHex(digest, 32, hex);
}

/* ---------------------------------------------------------------------------
 * Password storage
 *
 * A stored password looks like  <32 hex salt>:<64 hex hash>.
 * The salt is random and different for every account, so two people with the
 * same password get different stored values. The hash is SHA-256 repeated
 * many times, which makes guessing passwords slower.
 * ------------------------------------------------------------------------- */

#define SALT_BYTES 16
#define HASH_ROUNDS 10000
#define STORED_LEN (2 * SALT_BYTES + 1 + 64 + 1)

static void HashPassword(const unsigned char salt[SALT_BYTES], const char *password, char hex[65])
{
    unsigned char buffer[32 + SALT_BYTES + MAZE_AUTH_MAX_LEN];
    unsigned char digest[32] = {0};
    size_t length = strlen(password);

    for (int round = 0; round < HASH_ROUNDS; round++)
    {
        /* digest = SHA256(previous digest + salt + password) */
        memcpy(buffer, digest, 32);
        memcpy(buffer + 32, salt, SALT_BYTES);
        memcpy(buffer + 32 + SALT_BYTES, password, length);
        Sha256(buffer, 32 + SALT_BYTES + length, digest);
    }
    ToHex(digest, 32, hex);
}

/* Makes a fresh random salt and builds the text that goes in the database. */
static void MakeStoredPassword(const char *password, char stored[STORED_LEN])
{
    unsigned char salt[SALT_BYTES];
    char hash[65];

    sqlite3_randomness(SALT_BYTES, salt);   /* SQLite's own random source */
    HashPassword(salt, password, hash);
    ToHex(salt, SALT_BYTES, stored);
    stored[2 * SALT_BYTES] = ':';
    memcpy(stored + 2 * SALT_BYTES + 1, hash, 65);
}

static int HexValue(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/* Compares without stopping at the first difference. */
static int SameText(const char *a, const char *b)
{
    unsigned char difference = 0;
    size_t length = strlen(a);
    if (length != strlen(b)) return 0;
    for (size_t i = 0; i < length; i++)
        difference |= (unsigned char)(a[i] ^ b[i]);
    return difference == 0;
}

/* The very first version of this project stored a plain 64 bit FNV hash with
 * no salt. Old accounts still log in, and are upgraded when they do. */
static void LegacyHash(const char *password, char hex[32])
{
    uint64_t hash = 14695981039346656037ULL;
    for (; *password; password++)
    {
        hash ^= (unsigned char)(*password);
        hash *= 1099511628211ULL;
    }
    sprintf(hex, "%016llx", (unsigned long long)hash);
}

/* Returns 1 if the password matches what is stored. *legacy is set to 1 when
 * the stored value is the old unsalted format. */
static int CheckPassword(const char *password, const char *stored, int *legacy)
{
    *legacy = (strchr(stored, ':') == NULL);

    if (*legacy)
    {
        char hex[32];
        LegacyHash(password, hex);
        return SameText(hex, stored);
    }

    if (strlen(stored) != STORED_LEN - 1 || stored[2 * SALT_BYTES] != ':')
        return 0;

    unsigned char salt[SALT_BYTES];
    char hash[65];
    for (int i = 0; i < SALT_BYTES; i++)
    {
        int high = HexValue(stored[2 * i]);
        int low = HexValue(stored[2 * i + 1]);
        if (high < 0 || low < 0) return 0;
        salt[i] = (unsigned char)(high * 16 + low);
    }
    HashPassword(salt, password, hash);
    return SameText(hash, stored + 2 * SALT_BYTES + 1);
}

/* ---------------------------------------------------------------------------
 * Signup and login
 * ------------------------------------------------------------------------- */

static void SetMessage(char *message, size_t size, const char *text)
{
    snprintf(message, size, "%s", text);
}

int MazeAuth_Signup(const char *username, const char *password,
                    char *message, size_t size)
{
    char stored[STORED_LEN];
    sqlite3 *db;
    sqlite3_stmt *statement = NULL;
    int success = 0;

    if (username[0] == '\0' || password[0] == '\0')
    {
        SetMessage(message, size, "Username and password can't be empty.");
        return 0;
    }
    if (strlen(username) >= MAZE_AUTH_MAX_LEN || strlen(password) >= MAZE_AUTH_MAX_LEN)
    {
        SetMessage(message, size, "Username or password is too long.");
        return 0;
    }

    db = MazeDb_Open();
    if (!db)
    {
        SetMessage(message, size, "Could not open the database.");
        return 0;
    }

    MakeStoredPassword(password, stored);

    if (sqlite3_prepare_v2(db, "INSERT INTO users (username, password) VALUES (?, ?);",
                           -1, &statement, NULL) != SQLITE_OK)
    {
        SetMessage(message, size, "Database query error.");
    }
    else
    {
        sqlite3_bind_text(statement, 1, username, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 2, stored, -1, SQLITE_STATIC);

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
    char stored[128] = "";
    sqlite3 *db;
    sqlite3_stmt *statement = NULL;
    int found = 0;
    int success = 0;
    int legacy = 0;

    if (strlen(password) >= MAZE_AUTH_MAX_LEN)
    {
        SetMessage(message, size, "Wrong username or password.");
        return 0;
    }

    db = MazeDb_Open();
    if (!db)
    {
        SetMessage(message, size, "Could not open the database.");
        return 0;
    }

    if (sqlite3_prepare_v2(db, "SELECT password FROM users WHERE username = ?;",
                           -1, &statement, NULL) != SQLITE_OK)
    {
        SetMessage(message, size, "Database query error.");
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_text(statement, 1, username, -1, SQLITE_STATIC);
    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        snprintf(stored, sizeof stored, "%s", (const char *)sqlite3_column_text(statement, 0));
        found = 1;
    }
    sqlite3_finalize(statement);

    if (found && CheckPassword(password, stored, &legacy))
    {
        success = 1;
        SetMessage(message, size, "Login successful.");

        if (legacy)   /* move an old account to the salted format */
        {
            char upgraded[STORED_LEN];
            MakeStoredPassword(password, upgraded);
            if (sqlite3_prepare_v2(db, "UPDATE users SET password = ? WHERE username = ?;",
                                   -1, &statement, NULL) == SQLITE_OK)
            {
                sqlite3_bind_text(statement, 1, upgraded, -1, SQLITE_STATIC);
                sqlite3_bind_text(statement, 2, username, -1, SQLITE_STATIC);
                sqlite3_step(statement);
            }
            sqlite3_finalize(statement);
        }
    }
    else
    {
        SetMessage(message, size, "Wrong username or password.");
    }

    sqlite3_close(db);
    return success;
}
