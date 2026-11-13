/* menu.c - stand-alone login / sign up screen. The game has its own login screen;
   this one uses the same account code (maze_users.c). */
#include "raylib.h"
#include "maze.h"
#include <string.h>

#define SCREEN_WIDTH  1280
#define SCREEN_HEIGHT 720

/* which text box is selected */
#define FIELD_NONE 0
#define FIELD_USER 1
#define FIELD_PASS 2

/* draws a text box. if hide is true it shows **** instead of the text */
void DrawInputBox(Rectangle box, const char *text, bool selected, bool hide)
{
    char shown[MAX_LEN + 1];
    int i;

    if (selected)
    {
        DrawRectangleRec(box, (Color){40, 40, 60, 255});
        DrawRectangleLinesEx(box, 2, SKYBLUE);
    }
    else
    {
        DrawRectangleRec(box, (Color){30, 30, 30, 255});
        DrawRectangleLinesEx(box, 2, GRAY);
    }

    strcpy(shown, text);
    if (hide)
    {
        for (i = 0; shown[i] != '\0'; i++)
            shown[i] = '*';
    }

    DrawText(shown, box.x + 10, box.y + box.height / 2 - 10, 20, WHITE);
}

/* adds typed letters to the text and handles backspace */
void UpdateTextField(char *text)
{
    int len = strlen(text);
    int key = GetCharPressed();

    while (key > 0)
    {
        if (key >= 32 && key <= 125 && len < MAX_LEN - 1)
        {
            text[len] = (char)key;
            len++;
            text[len] = '\0';
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && len > 0)
        text[len - 1] = '\0';
}

bool DrawButton(Rectangle box, const char *text, Color color)
{
    bool hover = CheckCollisionPointRec(GetMousePosition(), box);
    int fontSize = 22;

    if (hover)
        DrawRectangleRec(box, color);
    else
        DrawRectangleRec(box, Fade(color, 0.7f));
    DrawRectangleLinesEx(box, 2, WHITE);

    DrawText(text, box.x + box.width / 2 - MeasureText(text, fontSize) / 2,
             box.y + box.height / 2 - fontSize / 2, fontSize, WHITE);

    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

int main(void)
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Graph Maze - Login");
    SetTargetFPS(60);

    Texture2D background = LoadTexture("assets/background.png");

    bool signupMode = true;    /* false = login mode */
    int selected = FIELD_NONE;
    char username[MAX_LEN] = "";
    char password[MAX_LEN] = "";
    char message[128] = "";

    Rectangle panel = {SCREEN_WIDTH / 2 - 220, SCREEN_HEIGHT / 2 - 200, 440, 400};
    Rectangle signupTab = {panel.x, panel.y, panel.width / 2, 50};
    Rectangle loginTab = {panel.x + panel.width / 2, panel.y, panel.width / 2, 50};
    Rectangle userBox = {panel.x + 30, panel.y + 100, panel.width - 60, 45};
    Rectangle passBox = {panel.x + 30, panel.y + 170, panel.width - 60, 45};
    Rectangle submitButton = {panel.x + 30, panel.y + 240, panel.width - 60, 50};
    Color tabColor = {40, 40, 50, 255};

    while (!WindowShouldClose())
    {
        /* ----- input ----- */
        Vector2 mouse = GetMousePosition();
        bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        if (clicked)
        {
            if (CheckCollisionPointRec(mouse, userBox))
                selected = FIELD_USER;
            else if (CheckCollisionPointRec(mouse, passBox))
                selected = FIELD_PASS;
            else
                selected = FIELD_NONE;

            if (CheckCollisionPointRec(mouse, signupTab))
            {
                signupMode = true;
                message[0] = '\0';
            }
            if (CheckCollisionPointRec(mouse, loginTab))
            {
                signupMode = false;
                message[0] = '\0';
            }
        }

        if (IsKeyPressed(KEY_TAB))
        {
            if (selected == FIELD_USER)
                selected = FIELD_PASS;
            else
                selected = FIELD_USER;
        }

        if (selected == FIELD_USER)
            UpdateTextField(username);
        else if (selected == FIELD_PASS)
            UpdateTextField(password);

        /* ----- draw ----- */
        BeginDrawing();
        ClearBackground(BLACK);

        DrawTexturePro(background, (Rectangle){0, 0, background.width, background.height},
                       (Rectangle){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}, (Vector2){0, 0}, 0, WHITE);

        DrawRectangleRec(panel, (Color){25, 25, 35, 180});
        DrawRectangleLinesEx(panel, 2, GRAY);

        /* tabs */
        DrawRectangleRec(signupTab, signupMode ? SKYBLUE : tabColor);
        DrawRectangleRec(loginTab, signupMode ? tabColor : SKYBLUE);
        DrawText("SIGN UP", signupTab.x + 20, signupTab.y + 15, 20, WHITE);
        DrawText("LOG IN", loginTab.x + 30, loginTab.y + 15, 20, WHITE);

        DrawText("Username:", userBox.x, userBox.y - 22, 16, LIGHTGRAY);
        DrawInputBox(userBox, username, selected == FIELD_USER, false);
        DrawText("Password:", passBox.x, passBox.y - 22, 16, LIGHTGRAY);
        DrawInputBox(passBox, password, selected == FIELD_PASS, true);

        if (DrawButton(submitButton, signupMode ? "CREATE ACCOUNT" : "LOG IN", (Color){70, 170, 255, 255}))
        {
            message[0] = '\0';
            if (signupMode)
                signup_user(username, password, message, sizeof(message));
            else
                login_user(username, password, message, sizeof(message));
        }

        if (message[0] != '\0')
            DrawText(message, panel.x + 30, submitButton.y + 70, 18, YELLOW);

        EndDrawing();
    }

    UnloadTexture(background);
    CloseWindow();
    return 0;
}
