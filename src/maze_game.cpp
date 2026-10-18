#include "raylib.h"

#include "maze_auth.h"
#include "maze_generator.h"
#include "maze_graph.h"
#include "maze_rules.h"
#include "maze_stats.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

namespace {

constexpr int screenWidth = 1280;
constexpr int screenHeight = 720;
constexpr int maxStamina = MAZE_STAMINA_MAX;

Vector2 CellCenter(int row, int col, int offsetX, int offsetY, int tileSize)
{
    return {
        static_cast<float>(offsetX + col * tileSize + tileSize / 2),
        static_cast<float>(offsetY + row * tileSize + tileSize / 2)
    };
}

} // namespace

enum class Screen {
    Login,
    Menu,
    Leaderboard,
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

// Text box for the login screen. Passwords are drawn as stars.
void DrawInputBox(Rectangle rect, const std::string& text, bool active, bool hidden)
{
    DrawRectangleRec(rect, active ? Color{40, 40, 60, 255} : Color{30, 30, 30, 255});
    DrawRectangleLinesEx(rect, 2, active ? SKYBLUE : GRAY);

    std::string shown = hidden ? std::string(text.size(), '*') : text;
    DrawText(shown.c_str(),
             static_cast<int>(rect.x + 10),
             static_cast<int>(rect.y + rect.height / 2 - 10),
             20,
             WHITE);
}

class Maze {
public:
    Maze()
    {
        MazeGraph_Init(&graph_, 1, 1);
    }

    ~Maze()
    {
        MazeGraph_Free(&graph_);
    }

    Maze(const Maze&) = delete;
    Maze& operator=(const Maze&) = delete;

    // Makes a new random maze. Returns false if it could not be built or solved.
    bool Generate(MazeDifficulty difficulty)
    {
        MazeDifficultyParams params;
        MazeGen_GetParams(difficulty, &params);

        rows_ = params.rows;
        cols_ = params.cols;
        fruitCount_ = params.fruit_count;
        grid_.assign(rows_ * cols_, 1);
        pathLength_ = 0;
        reachableCount_ = 0;

        unsigned int seed = static_cast<unsigned int>(GetRandomValue(1, 1000000000));
        if (!MazeGen_Generate(grid_.data(), rows_, cols_, params.loop_percent, seed))
            return false;
        if (!MazeGraph_BuildFromGrid(&graph_, grid_.data(), rows_, cols_))
            return false;

        std::vector<int> path(MAZE_GRAPH_MAX_CELLS);
        pathLength_ = MazeGraph_FindShortestPath(&graph_, StartRow(), StartCol(),
                                                 GoalRow(), GoalCol(),
                                                 path.data(), static_cast<int>(path.size()));
        reachableCount_ = MazeGraph_DfsReachableCount(&graph_, StartRow(), StartCol());
        return HasSolution();
    }

    bool IsWall(int row, int col) const
    {
        return grid_[row * cols_ + col] == 1;
    }

    bool HasSolution() const
    {
        return pathLength_ > 0 && reachableCount_ > 0;
    }

    int Rows() const { return rows_; }
    int Cols() const { return cols_; }
    int FruitCount() const { return fruitCount_; }

    // The generator always starts at (1, 1) and ends in the opposite corner.
    int StartRow() const { return 1; }
    int StartCol() const { return 1; }
    int GoalRow() const { return rows_ - 2; }
    int GoalCol() const { return cols_ - 2; }

private:
    MazeGraph graph_{};
    std::vector<int> grid_;
    int rows_ = 0;
    int cols_ = 0;
    int fruitCount_ = 0;
    int pathLength_ = 0;
    int reachableCount_ = 0;
};

class Player {
public:
    int row = 0;
    int col = 0;
    int stamina = maxStamina;
    int moves = 0;

    void Reset(const Maze& maze)
    {
        row = maze.StartRow();
        col = maze.StartCol();
        stamina = maxStamina;
        moves = 0;
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
        stamina = MazeRules_Move(stamina);
        moves++;
        return true;
    }
};

class Fruit {
public:
    Fruit(int row, int col, Texture2D icon)
        : row_(row), col_(col), icon_(icon) {}

    int Row() const { return row_; }
    int Col() const { return col_; }

    void Draw(int offsetX, int offsetY, int tileSize) const
    {
        if (!collected_)
        {
            Vector2 center = CellCenter(row_, col_, offsetX, offsetY, tileSize);
            float size = tileSize * 0.8f;
            Rectangle target = {center.x - size / 2, center.y - size / 2, size, size};

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
                DrawCircleV(center, size / 3, ORANGE);
            }
        }
    }

    void TryCollect(Player& player)
    {
        if (!collected_ && player.row == row_ && player.col == col_)
        {
            collected_ = true;
            player.stamina = MazeRules_Collect(player.stamina);
        }
    }

private:
    int row_;
    int col_;
    Texture2D icon_;
    bool collected_ = false;
};

class Game {
public:
    Game()
    {
        InitWindow(screenWidth, screenHeight, "MazeQuest - C and C++ Hybrid");
        SetTargetFPS(60);
        SetRandomSeed(static_cast<unsigned int>(std::time(nullptr)));

        background_ = LoadTexture("assets/image.png");
        shrub_ = LoadTexture("assets/bush-Photoroom.png");
        playerTexture_ = LoadTexture("resources/pixel people/astronaut 2.png");

        fruitIcons_[0] = LoadTexture("resources/scrimsy fruit icons/scrimsy fruit icons/kind/apple/red apple.png");
        fruitIcons_[1] = LoadTexture("resources/scrimsy fruit icons/scrimsy fruit icons/kind/banana/yellow banana.png");
        fruitIcons_[2] = LoadTexture("resources/scrimsy fruit icons/scrimsy fruit icons/kind/grape/purple grapes.png");
        fruitIcons_[3] = LoadTexture("resources/scrimsy fruit icons/scrimsy fruit icons/kind/strawberry/red strawberry.png");
    }

    ~Game()
    {
        for (Texture2D& icon : fruitIcons_)
            UnloadTexture(icon);

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
        if (screen_ == Screen::Login)
        {
            UpdateLogin();
            return;
        }

        if (screen_ == Screen::Menu || screen_ == Screen::Leaderboard)
        {
            return;
        }

        if (won_ || lost_)
        {
            if (IsKeyPressed(KEY_ENTER))
                screen_ = Screen::Menu;
            return;
        }

        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) player_.TryMove(-1, 0, maze_);
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) player_.TryMove(1, 0, maze_);
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) player_.TryMove(0, -1, maze_);
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) player_.TryMove(0, 1, maze_);

        for (Fruit& fruit : fruits_)
            fruit.TryCollect(player_);

        won_ = player_.row == maze_.GoalRow() && player_.col == maze_.GoalCol();
        lost_ = player_.stamina == 0 && !won_;

        if ((won_ || lost_) && !resultSaved_)
            SaveResult();
    }

    void Draw()
    {
        BeginDrawing();
        ClearBackground(BLACK);

        DrawBackground();

        if (screen_ == Screen::Login)
        {
            DrawLogin();
        }
        else if (screen_ == Screen::Menu)
        {
            DrawMenu();
        }
        else if (screen_ == Screen::Leaderboard)
        {
            DrawLeaderboard();
        }
        else
        {
            DrawMaze();
            DrawHud();

            if (won_) DrawText("YOU ESCAPED!", 515, 330, 40, GREEN);
            if (lost_) DrawText("STAMINA EMPTY", 500, 330, 40, RED);
            if (won_ || lost_) DrawText("Press ENTER for menu", 520, 380, 24, RAYWHITE);
            if (won_ || lost_) DrawText(ratingText_.c_str(), 520, 420, 22, YELLOW);
        }


        EndDrawing();
    }

    // Handles typing, clicks and Enter on the login screen.
    void UpdateLogin()
    {
        Vector2 mouse = GetMousePosition();

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            if (CheckCollisionPointRec(mouse, signupTab_))
            {
                signupMode_ = true;
                message_.clear();
            }
            else if (CheckCollisionPointRec(mouse, loginTab_))
            {
                signupMode_ = false;
                message_.clear();
            }

            if (CheckCollisionPointRec(mouse, userBox_)) field_ = Field::User;
            else if (CheckCollisionPointRec(mouse, passBox_)) field_ = Field::Pass;
            else field_ = Field::None;
        }

        if (IsKeyPressed(KEY_TAB))
            field_ = (field_ == Field::User) ? Field::Pass : Field::User;

        std::string* text = nullptr;
        if (field_ == Field::User) text = &username_;
        if (field_ == Field::Pass) text = &password_;

        if (text)
        {
            int letter = GetCharPressed();
            while (letter > 0)
            {
                if (letter >= 32 && letter <= 125 && text->size() < MAZE_AUTH_MAX_LEN - 1)
                    text->push_back(static_cast<char>(letter));
                letter = GetCharPressed();
            }

            if (IsKeyPressed(KEY_BACKSPACE) && !text->empty())
                text->pop_back();
        }

        if (IsKeyPressed(KEY_ENTER))
            SubmitLogin();
    }

    // Signs up or logs in, depending on the tab that is selected.
    void SubmitLogin()
    {
        char message[128];

        if (signupMode_)
        {
            if (MazeAuth_Signup(username_.c_str(), password_.c_str(), message, sizeof message))
            {
                signupMode_ = false;   // account created, now log in with it
                password_.clear();
            }
        }
        else if (MazeAuth_Login(username_.c_str(), password_.c_str(), message, sizeof message))
        {
            currentUser_ = username_;
            rating_ = MAZE_RATING_START;
            MazeStats_GetRating(currentUser_.c_str(), &rating_);
            password_.clear();
            message_.clear();
            screen_ = Screen::Menu;
            return;
        }

        message_ = message;
    }

    void DrawLogin()
    {
        DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 120});
        DrawText("MazeQuest", 505, 90, 54, RAYWHITE);

        DrawRectangleRec(loginPanel_, Color{25, 25, 35, 180});
        DrawRectangleLinesEx(loginPanel_, 2, GRAY);

        DrawRectangleRec(signupTab_, signupMode_ ? SKYBLUE : Color{40, 40, 50, 255});
        DrawRectangleRec(loginTab_, signupMode_ ? Color{40, 40, 50, 255} : SKYBLUE);
        DrawText("SIGN UP", static_cast<int>(signupTab_.x + 20), static_cast<int>(signupTab_.y + 15), 20, WHITE);
        DrawText("LOG IN", static_cast<int>(loginTab_.x + 30), static_cast<int>(loginTab_.y + 15), 20, WHITE);

        DrawText("Username:", static_cast<int>(userBox_.x), static_cast<int>(userBox_.y - 22), 16, LIGHTGRAY);
        DrawInputBox(userBox_, username_, field_ == Field::User, false);

        DrawText("Password:", static_cast<int>(passBox_.x), static_cast<int>(passBox_.y - 22), 16, LIGHTGRAY);
        DrawInputBox(passBox_, password_, field_ == Field::Pass, true);

        if (DrawButton(submitButton_, signupMode_ ? "CREATE ACCOUNT" : "LOG IN"))
            SubmitLogin();

        if (!message_.empty())
            DrawText(message_.c_str(), static_cast<int>(loginPanel_.x + 30), static_cast<int>(submitButton_.y + 70), 18, YELLOW);
    }

    // Saves the finished game once and works out the new rating.
    void SaveResult()
    {
        double before = rating_;
        resultSaved_ = true;

        if (MazeStats_RecordPlay(currentUser_.c_str(), static_cast<int>(difficulty_),
                                 maze_.Rows(), maze_.Cols(), won_, player_.moves,
                                 player_.stamina, &rating_))
        {
            char text[64];
            std::snprintf(text, sizeof text, "Rating %.0f -> %.0f (%+.0f)", before, rating_, rating_ - before);
            ratingText_ = text;
        }
        else
        {
            ratingText_ = "Could not save this game.";
        }
    }

    void OpenLeaderboard()
    {
        boardCount_ = MazeStats_Leaderboard(board_, MAZE_LEADERBOARD_MAX);
        if (boardCount_ < 0) boardCount_ = 0;
        boardMessage_.clear();
        screen_ = Screen::Leaderboard;
    }

    void DrawLeaderboard()
    {
        DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 120});
        DrawRectangle(300, 80, 680, 560, Color{25, 25, 35, 200});
        DrawRectangleLines(300, 80, 680, 560, GRAY);
        DrawText("LEADERBOARD", 520, 100, 32, RAYWHITE);

        DrawText("#", 330, 155, 18, LIGHTGRAY);
        DrawText("Player", 380, 155, 18, LIGHTGRAY);
        DrawText("Rating", 650, 155, 18, LIGHTGRAY);
        DrawText("Games", 760, 155, 18, LIGHTGRAY);
        DrawText("Wins", 860, 155, 18, LIGHTGRAY);

        for (int i = 0; i < boardCount_; i++)
        {
            int y = 188 + i * 30;
            Color color = (currentUser_ == board_[i].username) ? YELLOW : RAYWHITE;

            DrawText(TextFormat("%d", i + 1), 330, y, 20, color);
            DrawText(board_[i].username, 380, y, 20, color);
            DrawText(TextFormat("%.0f", board_[i].rating), 650, y, 20, color);
            DrawText(TextFormat("%d", board_[i].games), 760, y, 20, color);
            DrawText(TextFormat("%d", board_[i].wins), 860, y, 20, color);
        }

        if (boardCount_ == 0)
            DrawText("No players yet.", 380, 190, 20, LIGHTGRAY);

        if (DrawButton(Rectangle{330, 520, 200, 50}, "BACK"))
            screen_ = Screen::Menu;

        if (DrawButton(Rectangle{560, 520, 250, 50}, "EXPORT CSV"))
        {
            int rows = MazeStats_ExportCsv("play_log.csv");
            boardMessage_ = (rows >= 0)
                ? "Saved " + std::to_string(rows) + " games to play_log.csv"
                : "Could not write play_log.csv";
        }

        if (!boardMessage_.empty())
            DrawText(boardMessage_.c_str(), 330, 590, 18, YELLOW);
    }

    void DrawMenu()
    {
        DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 120});
        DrawText("MazeQuest", 505, 120, 54, RAYWHITE);
        DrawText(TextFormat("Logged in as %s   Rating: %.0f", currentUser_.c_str(), rating_), 460, 195, 22, RAYWHITE);

        Rectangle easyButton = {520, 240, 240, 54};
        Rectangle mediumButton = {520, 304, 240, 54};
        Rectangle hardButton = {520, 368, 240, 54};
        Rectangle boardButton = {520, 432, 240, 54};
        Rectangle logoutButton = {520, 496, 240, 54};
        Rectangle quitButton = {520, 560, 240, 54};

        if (DrawButton(easyButton, "EASY"))
            StartGame(MAZE_DIFFICULTY_EASY);

        if (DrawButton(mediumButton, "MEDIUM"))
            StartGame(MAZE_DIFFICULTY_MEDIUM);

        if (DrawButton(hardButton, "HARD"))
            StartGame(MAZE_DIFFICULTY_HARD);

        if (DrawButton(boardButton, "LEADERBOARD"))
            OpenLeaderboard();

        if (DrawButton(logoutButton, "LOG OUT"))
        {
            currentUser_.clear();
            rating_ = MAZE_RATING_START;
            username_.clear();
            password_.clear();
            message_.clear();
            field_ = Field::None;
            screen_ = Screen::Login;
        }

        if (DrawButton(quitButton, "QUIT"))
            quit_ = true;

        if (mazeFailed_)
            DrawText("Could not make a valid maze. Try again.", 460, 640, 20, RED);
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
            fruit.Draw(offsetX_, offsetY_, tileSize_);

        DrawPlayer();
        DrawCircleV(CellCenter(maze_.GoalRow(), maze_.GoalCol(), offsetX_, offsetY_, tileSize_),
                    tileSize_ / 3.0f, LIME);
    }

    void DrawPlayer()
    {
        Vector2 center = CellCenter(player_.row, player_.col, offsetX_, offsetY_, tileSize_);
        float size = static_cast<float>(tileSize_ - 2);
        Rectangle target = {center.x - size / 2, center.y - size / 2, size, size};

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
            DrawCircleV(center, size / 3, SKYBLUE);
        }
    }

    void DrawHud()
    {
        DrawText("Stamina", 36, 30, 20, RAYWHITE);
        DrawRectangle(36, 58, 240, 16, DARKGRAY);
        DrawRectangle(36, 58, static_cast<int>(240 * (player_.stamina / static_cast<float>(maxStamina))), 16, GREEN);
        DrawText(TextFormat("%d", player_.stamina), 288, 54, 22, RAYWHITE);
        DrawText(TextFormat("Player: %s", currentUser_.c_str()), 36, 90, 20, RAYWHITE);
    }

    // Puts the fruit on random open cells (not the start or the goal).
    void PlaceFruits()
    {
        fruits_.clear();

        while (static_cast<int>(fruits_.size()) < maze_.FruitCount())
        {
            int row = GetRandomValue(0, maze_.Rows() - 1);
            int col = GetRandomValue(0, maze_.Cols() - 1);

            if (maze_.IsWall(row, col)) continue;
            if (row == maze_.StartRow() && col == maze_.StartCol()) continue;
            if (row == maze_.GoalRow() && col == maze_.GoalCol()) continue;

            bool taken = false;
            for (const Fruit& fruit : fruits_)
            {
                if (fruit.Row() == row && fruit.Col() == col)
                    taken = true;
            }
            if (taken) continue;

            fruits_.push_back(Fruit(row, col, fruitIcons_[fruits_.size() % 4]));
        }
    }

    void StartGame(MazeDifficulty difficulty)
    {
        difficulty_ = difficulty;
        mazeFailed_ = !maze_.Generate(difficulty);
        if (mazeFailed_) return;

        // Fit the maze on screen and centre it.
        tileSize_ = (screenHeight - 40) / maze_.Rows();
        offsetX_ = (screenWidth - maze_.Cols() * tileSize_) / 2;
        offsetY_ = (screenHeight - maze_.Rows() * tileSize_) / 2;

        player_.Reset(maze_);
        PlaceFruits();

        won_ = false;
        lost_ = false;
        resultSaved_ = false;
        ratingText_.clear();
        screen_ = Screen::Playing;
    }

    enum class Field { None, User, Pass };

    Maze maze_;
    Player player_;

    // Login screen: layout, what has been typed, and who is logged in.
    Rectangle loginPanel_{420, 160, 440, 400};
    Rectangle signupTab_{420, 160, 220, 50};
    Rectangle loginTab_{640, 160, 220, 50};
    Rectangle userBox_{450, 260, 380, 45};
    Rectangle passBox_{450, 330, 380, 45};
    Rectangle submitButton_{450, 400, 380, 50};
    std::string username_;
    std::string password_;
    std::string message_;
    std::string currentUser_;
    double rating_ = MAZE_RATING_START;
    Field field_ = Field::None;
    bool signupMode_ = false;

    int tileSize_ = 30;
    int offsetX_ = 0;
    int offsetY_ = 0;
    Texture2D background_{};
    Texture2D shrub_{};
    Texture2D playerTexture_{};
    Texture2D fruitIcons_[4]{};
    std::vector<Fruit> fruits_;
    Screen screen_ = Screen::Login;
    MazeDifficulty difficulty_ = MAZE_DIFFICULTY_MEDIUM;
    bool resultSaved_ = false;
    std::string ratingText_;
    MazeLeaderboardEntry board_[MAZE_LEADERBOARD_MAX];
    int boardCount_ = 0;
    std::string boardMessage_;
    bool won_ = false;
    bool lost_ = false;
    bool mazeFailed_ = false;
    bool quit_ = false;
};

int main()
{
    Game game;
    game.Run();
    return 0;
}
