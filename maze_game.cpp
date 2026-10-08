#include "raylib.h"

#include "maze_generator.h"
#include "maze_graph.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

namespace {

constexpr int screenWidth = 1280;
constexpr int screenHeight = 720;

Vector2 CellCenter(int row, int col, int tileSize, int offsetX, int offsetY)
{
    return {
        static_cast<float>(offsetX + col * tileSize + tileSize / 2),
        static_cast<float>(offsetY + row * tileSize + tileSize / 2)
    };
}

} // namespace

enum class Screen {
    Menu,
    DifficultySelect,
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
    Maze() { BuildDefault(); }

    void Generate(MazeDifficulty diff, unsigned int seed = 0)
    {
        MazeGraph_Free(&graph_);

        MazeGen_GetParams(diff, &params_);
        rows_ = params_.rows;
        cols_ = params_.cols;
        grid_.resize(rows_ * cols_);

        MazeGen_Generate(grid_.data(), rows_, cols_, params_.loop_percent, seed);

        MazeGraph_Init(&graph_, rows_, cols_);
        MazeGraph_BuildFromGrid(&graph_, grid_.data(), rows_, cols_);

        path_.resize(MAZE_GRAPH_MAX_CELLS);
        pathLength_ = MazeGraph_FindShortestPath(&graph_, 1, 1,
                                                  rows_ - 2, cols_ - 2,
                                                  path_.data(),
                                                  static_cast<int>(path_.size()));
        reachableCount_ = MazeGraph_DfsReachableCount(&graph_, 1, 1);
    }

    ~Maze() { MazeGraph_Free(&graph_); }

    Maze(const Maze&) = delete;
    Maze& operator=(const Maze&) = delete;

    bool IsWall(int row, int col) const { return grid_[row * cols_ + col] == 1; }
    bool IsOpen(int row, int col) const { return grid_[row * cols_ + col] == 0; }
    bool HasSolution() const { return pathLength_ > 0 && reachableCount_ > 0; }

    int Rows() const { return rows_; }
    int Cols() const { return cols_; }
    int GoalRow() const { return rows_ - 2; }
    int GoalCol() const { return cols_ - 2; }
    const MazeDifficultyParams& Params() const { return params_; }

    void CollectOpenCells(std::vector<std::pair<int,int>> &out) const
    {
        out.clear();
        for (int r = 0; r < rows_; r++)
            for (int c = 0; c < cols_; c++)
                if (IsOpen(r, c) && !(r == 1 && c == 1) && !(r == GoalRow() && c == GoalCol()))
                    out.push_back({r, c});
    }

private:
    void BuildDefault()
    {
        rows_ = 21;
        cols_ = 21;
        params_ = {21, 21, 5, 4};
        grid_.resize(rows_ * cols_, 1);
        MazeGraph_Init(&graph_, rows_, cols_);
    }

    MazeGraph graph_{};
    MazeDifficultyParams params_{};
    std::vector<int> grid_;
    std::vector<int> path_;
    int rows_ = 21;
    int cols_ = 21;
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
        if (nextRow < 0 || nextRow >= maze.Rows() || nextCol < 0 || nextCol >= maze.Cols())
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

    void Load() { icon_ = LoadTexture(iconPath_); }

    void Unload()
    {
        if (icon_.id != 0)
            UnloadTexture(icon_);
    }

    void Draw(int tileSize, int offsetX, int offsetY) const
    {
        if (!collected_)
        {
            Vector2 center = CellCenter(row_, col_, tileSize, offsetX, offsetY);
            float half = tileSize * 0.4f;
            Rectangle target = {center.x - half, center.y - half, half * 2, half * 2};

            if (icon_.id != 0)
            {
                DrawTexturePro(icon_,
                    Rectangle{0, 0, static_cast<float>(icon_.width), static_cast<float>(icon_.height)},
                    target, Vector2{0, 0}, 0, WHITE);
            }
            else
            {
                DrawCircleV(center, tileSize * 0.27f, ORANGE);
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

    void Reset() { collected_ = false; }

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
    {
        InitWindow(screenWidth, screenHeight, "MazeQuest - C and C++ Hybrid");
        SetTargetFPS(60);
        background_ = LoadTexture("assets/image.png");
        shrub_ = LoadTexture("assets/bush-Photoroom.png");
        playerTexture_ = LoadTexture("resources/pixel people/astronaut 2.png");
    }

    ~Game()
    {
        UnloadFruits();
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
    void RecalcLayout()
    {
        int maxTileW = screenWidth / maze_.Cols();
        int maxTileH = screenHeight / maze_.Rows();
        tileSize_ = std::min(maxTileW, maxTileH);
        offsetX_ = (screenWidth - maze_.Cols() * tileSize_) / 2;
        offsetY_ = (screenHeight - maze_.Rows() * tileSize_) / 2;
    }

    void SpawnFruits()
    {
        UnloadFruits();
        fruits_.clear();

        const char *iconPaths[] = {
            "resources/scrimsy fruit icons/scrimsy fruit icons/kind/apple/red apple.png",
            "resources/scrimsy fruit icons/scrimsy fruit icons/kind/banana/yellow banana.png",
            "resources/scrimsy fruit icons/scrimsy fruit icons/kind/grape/purple grapes.png",
            "resources/scrimsy fruit icons/scrimsy fruit icons/kind/strawberry/red strawberry.png",
            "resources/scrimsy fruit icons/scrimsy fruit icons/kind/apple/red apple.png"
        };
        int numIcons = 5;

        std::vector<std::pair<int,int>> openCells;
        maze_.CollectOpenCells(openCells);

        unsigned int seed = static_cast<unsigned int>(time(nullptr));
        srand(seed);
        for (int i = static_cast<int>(openCells.size()) - 1; i > 0; i--)
        {
            int j = rand() % (i + 1);
            std::swap(openCells[i], openCells[j]);
        }

        int count = std::min(maze_.Params().fruit_count, static_cast<int>(openCells.size()));
        for (int i = 0; i < count; i++)
        {
            fruits_.emplace_back(openCells[i].first, openCells[i].second,
                                 iconPaths[i % numIcons]);
        }

        for (Fruit& fruit : fruits_)
            fruit.Load();
    }

    void UnloadFruits()
    {
        for (Fruit& fruit : fruits_)
            fruit.Unload();
    }

    void Update()
    {
        if (screen_ == Screen::Menu || screen_ == Screen::DifficultySelect)
            return;

        if (won_ || lost_) return;

        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) player_.TryMove(-1, 0, maze_);
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) player_.TryMove(1, 0, maze_);
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) player_.TryMove(0, -1, maze_);
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) player_.TryMove(0, 1, maze_);

        for (Fruit& fruit : fruits_)
            fruit.TryCollect(player_);

        won_ = player_.row == maze_.GoalRow() && player_.col == maze_.GoalCol();
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
        else if (screen_ == Screen::DifficultySelect)
        {
            DrawDifficultySelect();
        }
        else
        {
            DrawMaze();
            DrawHud();

            if (won_) DrawCenteredText("YOU ESCAPED!", 40, GREEN);
            if (lost_) DrawCenteredText("STAMINA EMPTY", 40, RED);
        }

        EndDrawing();
    }

    void DrawCenteredText(const char *text, int fontSize, Color color)
    {
        int w = MeasureText(text, fontSize);
        DrawText(text, screenWidth / 2 - w / 2, screenHeight / 2 - fontSize / 2, fontSize, color);
    }

    void DrawMenu()
    {
        DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 120});
        DrawText("MazeQuest", 505, 170, 54, RAYWHITE);

        Rectangle startButton = {520, 310, 240, 58};
        Rectangle quitButton = {520, 390, 240, 58};

        if (DrawButton(startButton, "START GAME"))
            screen_ = Screen::DifficultySelect;

        if (DrawButton(quitButton, "QUIT"))
            quit_ = true;
    }

    void DrawDifficultySelect()
    {
        DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 120});
        DrawText("Select Difficulty", 450, 170, 44, RAYWHITE);

        Rectangle easyBtn   = {520, 280, 240, 58};
        Rectangle mediumBtn = {520, 360, 240, 58};
        Rectangle hardBtn   = {520, 440, 240, 58};

        if (DrawButton(easyBtn, "EASY"))
            StartWithDifficulty(MAZE_DIFFICULTY_EASY);

        if (DrawButton(mediumBtn, "MEDIUM"))
            StartWithDifficulty(MAZE_DIFFICULTY_MEDIUM);

        if (DrawButton(hardBtn, "HARD"))
            StartWithDifficulty(MAZE_DIFFICULTY_HARD);
    }

    void StartWithDifficulty(MazeDifficulty diff)
    {
        unsigned int seed = static_cast<unsigned int>(time(nullptr));
        maze_.Generate(diff, seed);

        if (!maze_.HasSolution())
            maze_.Generate(diff, seed + 1);

        RecalcLayout();
        SpawnFruits();
        player_.Reset();
        won_ = false;
        lost_ = false;
        screen_ = Screen::Playing;
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
        for (int row = 0; row < maze_.Rows(); row++)
        {
            for (int col = 0; col < maze_.Cols(); col++)
            {
                Rectangle tile = {
                    static_cast<float>(offsetX_ + col * tileSize_),
                    static_cast<float>(offsetY_ + row * tileSize_),
                    static_cast<float>(tileSize_),
                    static_cast<float>(tileSize_)
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
            fruit.Draw(tileSize_, offsetX_, offsetY_);

        DrawPlayer();

        Vector2 goalCenter = CellCenter(maze_.GoalRow(), maze_.GoalCol(), tileSize_, offsetX_, offsetY_);
        DrawCircleV(goalCenter, tileSize_ * 0.33f, LIME);
    }

    void DrawPlayer()
    {
        Vector2 center = CellCenter(player_.row, player_.col, tileSize_, offsetX_, offsetY_);
        float half = tileSize_ * 0.47f;
        Rectangle target = {center.x - half, center.y - half, half * 2, half * 2};

        if (playerTexture_.id != 0)
        {
            DrawTexturePro(playerTexture_,
                Rectangle{0, 0, static_cast<float>(playerTexture_.width), static_cast<float>(playerTexture_.height)},
                target, Vector2{0, 0}, 0, WHITE);
        }
        else
        {
            DrawCircleV(center, tileSize_ * 0.33f, SKYBLUE);
        }
    }

    void DrawHud()
    {
        DrawText("Stamina", 36, 30, 20, RAYWHITE);
        DrawRectangle(36, 58, 240, 16, DARKGRAY);
        DrawRectangle(36, 58, static_cast<int>(240 * (player_.stamina / 100.0f)), 16, GREEN);
        DrawText(TextFormat("%d", player_.stamina), 288, 54, 22, RAYWHITE);
    }

    Maze maze_;
    Player player_;
    int tileSize_ = 30;
    int offsetX_ = 0;
    int offsetY_ = 0;
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
