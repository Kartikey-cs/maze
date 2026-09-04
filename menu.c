#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define SCREEN_WIDTH  1280
#define SCREEN_HEIGHT 720
#define MAX_LEN 64
#define USERS_FILE "users.txt"

typedef enum { MODE_SIGNUP, MODE_LOGIN } Mode;
typedef enum { FIELD_NONE, FIELD_USER, FIELD_PASS } Field;


uint64_t HashPassword(const char *str)
{
    uint64_t hash = 14695981039346656037ULL;
    for (; *str; str++)
    {
        hash ^= (unsigned char)(*str);
        hash *= 1099511628211ULL;
    }
    return hash;
}

// Text input box:

void DrawInputBox(Rectangle rect, const char *text, bool active, bool mask)
{
    DrawRectangleRec(rect, active ? (Color){40,40,60,255} : (Color){30,30,30,255});
    DrawRectangleLinesEx(rect, 2, active ? SKYBLUE : GRAY);

    char display[MAX_LEN + 1];
    if (mask)
    {
        int len = (int)strlen(text);
        for (int i = 0; i < len; i++) display[i] = '*';
        display[len] = '\0';
    }
    else
    {
        strcpy(display, text);
    }

    DrawText(display, rect.x + 10, rect.y + rect.height / 2 - 10, 20, WHITE);
}

void UpdateTextField(char *buffer)
{
    int letter = GetCharPressed();
    int len = (int)strlen(buffer);

    while (letter > 0)
    {
        if ((letter >= 32) && (letter <= 125) && (len < MAX_LEN - 1))
        {
            buffer[len] = (char)letter;
            buffer[len + 1] = '\0';
            len++;
        }
        letter = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && len > 0)
        buffer[len - 1] = '\0';
}


// Button 

bool DrawButton(Rectangle rect, const char *text, Color base)
{
    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, rect);

    DrawRectangleRec(rect, hover ? Fade(base, 1.0f) : Fade(base, 0.7f));
    DrawRectangleLinesEx(rect, 2, WHITE);

    int fontSize = 22;
    DrawText(text,
        rect.x + rect.width / 2 - MeasureText(text, fontSize) / 2,
        rect.y + rect.height / 2 - fontSize / 2,
        fontSize, WHITE);

    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}


// File storage: "username:hashhex" per line

bool UsernameExists(const char *username)
{
    FILE *f = fopen(USERS_FILE, "r");
    if (!f) return false;

    char fileUser[MAX_LEN];
    char fileHash[32];
    bool found = false;

    while (fscanf(f, "%63[^:]:%31s\n", fileUser, fileHash) == 2)
    {
        if (strcmp(fileUser, username) == 0)
        {
            found = true;
            break;
        }
    }

    fclose(f);
    return found;
}

bool SignUp(const char *username, const char *password, char *message)
{
    if (strlen(username) == 0 || strlen(password) == 0)
    {
        strcpy(message, "Username/password can't be empty");
        return false;
    }

    if (UsernameExists(username))
    {
        strcpy(message, "Username already exists");
        return false;
    }

    FILE *f = fopen(USERS_FILE, "a");
    if (!f)
    {
        strcpy(message, "Could not open users file");
        return false;
    }

    uint64_t hash = HashPassword(password);
    fprintf(f, "%s:%016llx\n", username, (unsigned long long)hash);
    fclose(f);

    strcpy(message, "Account created! You can log in now.");
    return true;
}

bool LogIn(const char *username, const char *password, char *message)
{
    FILE *f = fopen(USERS_FILE, "r");
    if (!f)
    {
        strcpy(message, "No accounts yet, sign up first");
        return false;
    }

    char fileUser[MAX_LEN];
    char fileHash[32];
    uint64_t enteredHash = HashPassword(password);
    char enteredHex[32];
    sprintf(enteredHex, "%016llx", (unsigned long long)enteredHash);

    bool success = false;

    while (fscanf(f, "%63[^:]:%31s\n", fileUser, fileHash) == 2)
    {
        if (strcmp(fileUser, username) == 0)
        {
            if (strcmp(fileHash, enteredHex) == 0)
            {
                strcpy(message, "Login successful!");
                success = true;
            }
            else
            {
                strcpy(message, "Wrong password");
            }
            break;
        }
    }

    if (!success && strlen(message) == 0)
        strcpy(message, "Username not found");

    fclose(f);
    return success;
}

int main(void)
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Graph Maze - Login");
    SetTargetFPS(60);

    Texture2D background = LoadTexture("assets/background.png");

    Mode mode = MODE_SIGNUP;
    Field active = FIELD_NONE;

    char username[MAX_LEN] = "";
    char password[MAX_LEN] = "";
    char message[128] = "";

    Rectangle panel = { SCREEN_WIDTH/2 - 220, SCREEN_HEIGHT/2 - 200, 440, 400 };

    Rectangle signupTab = { panel.x, panel.y, panel.width/2, 50 };
    Rectangle loginTab  = { panel.x + panel.width/2, panel.y, panel.width/2, 50 };

    Rectangle userBox = { panel.x + 30, panel.y + 100, panel.width - 60, 45 };
    Rectangle passBox = { panel.x + 30, panel.y + 170, panel.width - 60, 45 };

    Rectangle submitBtn = { panel.x + 30, panel.y + 240, panel.width - 60, 50 };

    while (!WindowShouldClose())
    {

        // Input handling

        Vector2 mouse = GetMousePosition();

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            if (CheckCollisionPointRec(mouse, userBox)) active = FIELD_USER;
            else if (CheckCollisionPointRec(mouse, passBox)) active = FIELD_PASS;
            else active = FIELD_NONE;
        }

        if (IsKeyPressed(KEY_TAB))
            active = (active == FIELD_USER) ? FIELD_PASS : FIELD_USER;

        if (active == FIELD_USER) UpdateTextField(username);
        else if (active == FIELD_PASS) UpdateTextField(password);


        // Draw

        BeginDrawing();
        ClearBackground(BLACK);

        DrawTexturePro(
            background,
            (Rectangle){0, 0, background.width, background.height},
            (Rectangle){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT},
            (Vector2){0, 0},
            0,
            WHITE
        );

        DrawRectangleRec(panel, (Color){25,25,35,180});
        DrawRectangleLinesEx(panel, 2, GRAY);

        // Tabs to switch between Sign Up / Log In
        DrawRectangleRec(signupTab, mode == MODE_SIGNUP ? SKYBLUE : (Color){40,40,50,255});
        DrawRectangleRec(loginTab,  mode == MODE_LOGIN  ? SKYBLUE : (Color){40,40,50,255});
        DrawText("SIGN UP", signupTab.x + 20, signupTab.y + 15, 20, WHITE);
        DrawText("LOG IN",  loginTab.x + 30,  loginTab.y + 15,  20, WHITE);

        if (CheckCollisionPointRec(mouse, signupTab) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            mode = MODE_SIGNUP;
            message[0] = '\0';
        }
        if (CheckCollisionPointRec(mouse, loginTab) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            mode = MODE_LOGIN;
            message[0] = '\0';
        }

        DrawText("Username:", userBox.x, userBox.y - 22, 16, LIGHTGRAY);
        DrawInputBox(userBox, username, active == FIELD_USER, false);

        DrawText("Password:", passBox.x, passBox.y - 22, 16, LIGHTGRAY);
        DrawInputBox(passBox, password, active == FIELD_PASS, true);

        const char *btnLabel = (mode == MODE_SIGNUP) ? "CREATE ACCOUNT" : "LOG IN";
        if (DrawButton(submitBtn, btnLabel, (Color){70,170,255,255}))
        {
            message[0] = '\0';
            if (mode == MODE_SIGNUP)
                SignUp(username, password, message);
            else
                LogIn(username, password, message);
        }

        if (strlen(message) > 0)
            DrawText(message, panel.x + 30, submitBtn.y + 70, 18, YELLOW);

        EndDrawing();
    }

    UnloadTexture(background);
    CloseWindow();
    return 0;
}