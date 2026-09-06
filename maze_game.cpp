#include "raylib.h"

#include "maze_graph.h"

#include <algorithm>
#include <array>
#include <vector>

namespace {

constexpr int screenWidth = 1280;
constexpr int screenHeight = 720;
constexpr int rows = 21;
constexpr int cols = 21;
constexpr int tileSize = 30;

const std::array<int, rows * cols> mazeGrid = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,0,1,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,1,
    1,0,1,0,1,1,1,0,1,0,1,0,1,0,1,1,1,1,1,0,1,
    1,0,0,0,1,0,0,0,1,0,1,0,0,0,1,0,0,0,1,0,1,
    1,1,1,0,1,0,1,1,1,0,1,1,1,1,1,0,1,0,1,0,1,
    1,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,1,0,1,0,1,
    1,0,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,0,1,0,1,
    1,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,1,0,1,0,1,
    1,0,1,0,1,0,1,1,1,1,1,1,1,1,1,0,1,0,1,0,1,
    1,0,1,0,1,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1,
    1,0,1,0,1,1,1,1,1,1,1,0,1,0,1,1,1,1,1,0,1,
    1,0,1,0,0,0,0,0,1,0,0,0,1,0,1,0,0,0,1,0,1,
    1,0,1,1,1,1,1,0,1,0,1,1,1,0,1,0,1,0,1,0,1,
    1,0,0,0,0,0,1,0,1,0,0,0,1,0,1,0,1,0,0,0,1,
    1,1,1,1,1,0,1,0,1,1,1,0,1,0,1,0,1,1,1,0,1,
    1,0,0,0,1,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,1,
    1,0,1,0,1,0,1,1,1,0,1,1,1,1,1,1,1,0,1,0,1,
    1,0,1,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,1,0,1,
    1,0,1,1,1,1,1,0,1,1,1,1,1,1,1,0,1,0,1,0,1,
    1,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
};

Vector2 CellCenter(int row, int col, int offsetX, int offsetY)
{
    return {
        static_cast<float>(offsetX + col * tileSize + tileSize / 2),
        static_cast<float>(offsetY + row * tileSize + tileSize / 2)
    };
}

} // namespace

enum class Screen {
    Menu,
    Playing
};

bool DrawButton(Rectangle rect, const char *text)
{
    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, rect);

    DrawRectangleRec(rect, hover ? Color{80, 150, 210, 255} : Color{50, 95, 145, 255});
    DrawRectangleLinesEx(rect, 2, RAYWHITE);

    int fontSize = 24;
    int textWidth = MeasureText(text, fontSize);
    DrawText(text,
             static_cast<int>(rect.x + rect.width / 2 - textWidth / 2),
             static_cast<int>(rect.y + rect.height / 2 - fontSize / 2),
             fontSize,
             RAYWHITE);

    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

class Maze {
public:
    Maze()
    {
        MazeGraph_Init(&graph_, rows, cols);
        MazeGraph_BuildFromGrid(&graph_, mazeGrid.data(), rows, cols);
        path_.resize(MAZE_GRAPH_MAX_CELLS);
        pathLength_ = MazeGraph_FindShortestPath(&graph_, 1, 1, 19, 19,
                                                 path_.data(), static_cast<int>(path_.size()));
        reachableCount_ = MazeGraph_DfsReachableCount(&graph_, 1, 1);
    }

    ~Maze()
    {
        MazeGraph_Free(&graph_);
    }

    Maze(const Maze&) = delete;
    Maze& operator=(const Maze&) = delete;

    bool IsWall(int row, int col) const
    {
        return mazeGrid[row * cols + col] == 1;
    }

    bool HasSolution() const
    {
        return pathLength_ > 0 && reachableCount_ > 0;
    }

private:
    MazeGraph graph_{};
    std::vector<int> path_;
    int pathLength_ = 0;
    int reachableCount_ = 0;
};

class Player {
public:
    int row = 1;
    int col = 1;
    int stamina = 100;

    void Reset()
    {
        row = 1;
        col = 1;
        stamina = 100;
    }

    bool TryMove(int dRow, int dCol, const Maze& maze)
    {
        int nextRow = row + dRow;
        int nextCol = col + dCol;
        if (nextRow < 0 || nextRow >= rows || nextCol < 0 || nextCol >= cols)
            return false;
        if (maze.IsWall(nextRow, nextCol) || stamina <= 0)
            return false;

        row = nextRow;
        col = nextCol;
        stamina = std::max(0, stamina - 1);
        return true;
    }
};

class Fruit {
public:
    Fruit(int row, int col, const char *iconPath)
        : row_(row), col_(col), iconPath_(iconPath) {}

    void Load()
    {
        icon_ = LoadTexture(iconPath_);
    }

    void Unload()
    {
        if (icon_.id != 0)
            UnloadTexture(icon_);
    }

    void Draw(int offsetX, int offsetY) const
    {
        if (!collected_)
        {
            Vector2 center = CellCenter(row_, col_, offsetX, offsetY);
            Rectangle target = {center.x - 12, center.y - 12, 24, 24};

            if (icon_.id != 0)
            {
                DrawTexturePro(icon_,
                    Rectangle{0, 0, static_cast<float>(icon_.width), static_cast<float>(icon_.height)},
                    target,
                    Vector2{0, 0},
                    0,
                    WHITE);
            }
            else
            {
                DrawCircleV(center, 8, ORANGE);
            }
        }
    }

    void TryCollect(Player& player)
    {
        if (!collected_ && player.row == row_ && player.col == col_)
        {
            collected_ = true;
            player.stamina = std::min(100, player.stamina + 20);
        }
    }

    void Reset()
    {
        collected_ = false;
    }

private:
    int row_;
    int col_;
    const char *iconPath_;
    Texture2D icon_{};
    bool collected_ = false;
};

class Game {
public:
    Game()
        : offsetX_((screenWidth - cols * tileSize) / 2),
          offsetY_((screenHeight - rows * tileSize) / 2),
          fruits_{
              {5, 1, "resources/scrimsy fruit icons/scrimsy fruit icons/kind/apple/red apple.png"},
              {9, 9, "resources/scrimsy fruit icons/scrimsy fruit icons/kind/banana/yellow banana.png"},
              {15, 3, "resources/scrimsy fruit icons/scrimsy fruit icons/kind/grape/purple grapes.png"},
              {17, 13, "resources/scrimsy fruit icons/scrimsy fruit icons/kind/strawberry/red strawberry.png"}
          }
    {
        InitWindow(screenWidth, screenHeight, "MazeQuest - C and C++ Hybrid");
        SetTargetFPS(60);
        background_ = LoadTexture("assets/image.png");
        shrub_ = LoadTexture("assets/bush-Photoroom.png");
        playerTexture_ = LoadTexture("resources/pixel people/astronaut 2.png");

        for (Fruit& fruit : fruits_)
            fruit.Load();
    }

    ~Game()
    {
        for (Fruit& fruit : fruits_)
            fruit.Unload();

        UnloadTexture(shrub_);
        UnloadTexture(background_);
        UnloadTexture(playerTexture_);
        CloseWindow();
    }

    void Run()
    {
        while (!WindowShouldClose() && !quit_)
        {
            Update();
            Draw();
        }
    }

private:
    void Update()
    {
        if (screen_ == Screen::Menu)
        {
            return;
        }

        if (won_ || lost_) return;

        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) player_.TryMove(-1, 0, maze_);
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) player_.TryMove(1, 0, maze_);
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) player_.TryMove(0, -1, maze_);
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) player_.TryMove(0, 1, maze_);

        for (Fruit& fruit : fruits_)
            fruit.TryCollect(player_);

        won_ = player_.row == 19 && player_.col == 19;
        lost_ = player_.stamina == 0 && !won_;
    }

    void Draw()
    {
        BeginDrawing();
        ClearBackground(BLACK);

        DrawBackground();

        if (screen_ == Screen::Menu)
        {
            DrawMenu();
        }
        else
        {
            DrawMaze();
            DrawHud();

            if (won_) DrawText("YOU ESCAPED!", 515, 330, 40, GREEN);
            if (lost_) DrawText("STAMINA EMPTY", 500, 330, 40, RED);
        }


        EndDrawing();
    }

    void DrawMenu()
    {
        DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 120});
        DrawText("MazeQuest", 505, 170, 54, RAYWHITE);

        Rectangle startButton = {520, 310, 240, 58};
        Rectangle quitButton = {520, 390, 240, 58};

        if (DrawButton(startButton, "START GAME"))
            StartGame();

        if (DrawButton(quitButton, "QUIT"))
            quit_ = true;

        if (!maze_.HasSolution())
            DrawText("Maze has no valid path.", 520, 470, 20, RED);
    }

    void DrawBackground()
    {
        if (background_.id != 0)
        {
            DrawTexturePro(background_,
                Rectangle{0, 0, static_cast<float>(background_.width), static_cast<float>(background_.height)},
                Rectangle{0, 0, screenWidth, screenHeight},
                Vector2{0, 0}, 0, WHITE);
        }
        else
        {
            ClearBackground(Color{22, 34, 29, 255});
        }
    }

    void DrawMaze()
    {
        for (int row = 0; row < rows; row++)
        {
            for (int col = 0; col < cols; col++)
            {
                Rectangle tile = {
                    static_cast<float>(offsetX_ + col * tileSize),
                    static_cast<float>(offsetY_ + row * tileSize),
                    static_cast<float>(tileSize),
                    static_cast<float>(tileSize)
                };

                if (maze_.IsWall(row, col))
                {
                    if (shrub_.id != 0)
                    {
                        DrawTexturePro(shrub_,
                            Rectangle{0, 0, static_cast<float>(shrub_.width), static_cast<float>(shrub_.height)},
                            tile, Vector2{0, 0}, 0, WHITE);
                    }
                    else
                    {
                        DrawRectangleRec(tile, DARKGREEN);
                    }
                }
                else
                {
                    DrawRectangleRec(tile, Color{12, 14, 18, 115});
                }
            }
        }

        for (const Fruit& fruit : fruits_)
            fruit.Draw(offsetX_, offsetY_);

        DrawPlayer();
        DrawCircleV(CellCenter(19, 19, offsetX_, offsetY_), 10, LIME);
    }

    void DrawPlayer()
    {
        Vector2 center = CellCenter(player_.row, player_.col, offsetX_, offsetY_);
        Rectangle target = {center.x - 14, center.y - 14, 28, 28};

        if (playerTexture_.id != 0)
        {
            DrawTexturePro(playerTexture_,
                Rectangle{0, 0, static_cast<float>(playerTexture_.width), static_cast<float>(playerTexture_.height)},
                target,
                Vector2{0, 0},
                0,
                WHITE);
        }
        else
        {
            DrawCircleV(center, 10, SKYBLUE);
        }
    }

    void DrawHud()
    {
        DrawText("Stamina", 36, 30, 20, RAYWHITE);
        DrawRectangle(36, 58, 240, 16, DARKGRAY);
        DrawRectangle(36, 58, static_cast<int>(240 * (player_.stamina / 100.0f)), 16, GREEN);
        DrawText(TextFormat("%d", player_.stamina), 288, 54, 22, RAYWHITE);
    }

    void StartGame()
    {
        player_.Reset();
        for (Fruit& fruit : fruits_)
            fruit.Reset();

        won_ = false;
        lost_ = false;
        screen_ = Screen::Playing;
    }

    Maze maze_;
    Player player_;
    int offsetX_;
    int offsetY_;
    Texture2D background_{};
    Texture2D shrub_{};
    Texture2D playerTexture_{};
    std::vector<Fruit> fruits_;
    Screen screen_ = Screen::Menu;
    bool won_ = false;
    bool lost_ = false;
    bool quit_ = false;
};

int main()
{
    Game game;
    game.Run();
    return 0;
}
