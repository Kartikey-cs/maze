#include "raylib.h"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define ROWS 21
#define COLS 21
#define TILE_SIZE ((SCREEN_HEIGHT < SCREEN_WIDTH ? SCREEN_HEIGHT : SCREEN_WIDTH) / ROWS)

int main(void)
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Maze Test");
    SetTargetFPS(60);
    int offsetX = (SCREEN_WIDTH - COLS * TILE_SIZE) / 2;
    int offsetY = (SCREEN_HEIGHT - ROWS * TILE_SIZE) / 2;

    // Load textures
    Texture2D background = LoadTexture("assets/image.png");
    Texture2D shrub = LoadTexture("assets/bush-Photoroom.png");

    // Check if textures loaded
    if (background.id == 0 || shrub.id == 0)
    {
        CloseWindow();
        return 1;
    }

    int maze[ROWS][COLS] =
{
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,0,1,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,1},
{1,0,1,0,1,1,1,0,1,0,1,0,1,0,1,1,1,1,1,0,1},
{1,0,0,0,1,0,0,0,1,0,1,0,0,0,1,0,0,0,1,0,1},
{1,1,1,0,1,0,1,1,1,0,1,1,1,1,1,0,1,0,1,0,1},
{1,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,1,0,1,0,1},
{1,0,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,0,1,0,1},
{1,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,1,0,1,0,1},
{1,0,1,0,1,0,1,1,1,1,1,1,1,1,1,0,1,0,1,0,1},
{1,0,1,0,1,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1},
{1,0,1,0,1,1,1,1,1,1,1,0,1,0,1,1,1,1,1,0,1},
{1,0,1,0,0,0,0,0,1,0,0,0,1,0,1,0,0,0,1,0,1},
{1,0,1,1,1,1,1,0,1,0,1,1,1,0,1,0,1,0,1,0,1},
{1,0,0,0,0,0,1,0,1,0,0,0,1,0,1,0,1,0,0,0,1},
{1,1,1,1,1,0,1,0,1,1,1,0,1,0,1,0,1,1,1,0,1},
{1,0,0,0,1,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,1},
{1,0,1,0,1,0,1,1,1,0,1,1,1,1,1,1,1,0,1,0,1},
{1,0,1,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,1,0,1},
{1,0,1,1,1,1,1,0,1,1,1,1,1,1,1,0,1,0,1,0,1},
{1,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

    while (!WindowShouldClose())
    {
        BeginDrawing();

        ClearBackground(BLACK);

        // Draw background
        DrawTexturePro(
            background,
            (Rectangle){0, 0, background.width, background.height},
            (Rectangle){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT},
            (Vector2){0, 0},
            0,
            WHITE
        );

        // Draw maze
        for (int y = 0; y < ROWS; y++)
        {
            for (int x = 0; x < COLS; x++)
            {
                if (maze[y][x] == 1)
                {
                    DrawTexturePro(
                        shrub,
                    (Rectangle){0,0,shrub.width,shrub.height},
                    (Rectangle)
                    {
                        offsetX + x * TILE_SIZE,
                        offsetY + y * TILE_SIZE,
                        TILE_SIZE,
                        TILE_SIZE
                    },
                    (Vector2){0,0},
                    0,
                    WHITE
                    );
                }
            }
        }

        EndDrawing();
    }

    UnloadTexture(shrub);
    UnloadTexture(background);

    CloseWindow();

    return 0;
}