#include "raylib.h"
#define X 1280
#define Y 720
int main()
{
    InitWindow(X,Y,"Graph Maze");

    SetTargetFPS(60);

    Texture2D background = LoadTexture("assets/image.png");//main menu background
    while(!WindowShouldClose())
    {
    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(
    background,
    (Rectangle){0,0,background.width,background.height},//fill
    (Rectangle){0,0,X,Y},//whole window
    (Vector2){0,0},0,WHITE);
    DrawRectangle(0,0,X,Y,Fade(BLACK, 0.35f));
    EndDrawing();
    }
    UnloadTexture(background);
    CloseWindow();
}