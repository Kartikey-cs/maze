// maze_game.cpp - the playable game (C++ classes + raylib).
// The maze, planner and accounts are C code (maze_core.c, maze_generator.c, maze_users.c).
#include "raylib.h"
#include "maze.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define SOLVER_COUNT 4

// which screen is showing
#define SCREEN_LOGIN 0
#define SCREEN_MENU 1
#define SCREEN_LEADERBOARD 2
#define SCREEN_PLAYING 3

// which text box is selected on the login screen
#define FIELD_NONE 0
#define FIELD_USER 1
#define FIELD_PASS 2

const char *difficultyNames[3] = {"Easy", "Medium", "Hard"};

// ------------------------------------------------------------------
// small drawing helpers

// middle of a cell on the screen
Vector2 CellCenter(int row, int col, int offsetX, int offsetY, int tileSize)
{
    Vector2 v;
    v.x = (float)(offsetX + col * tileSize + tileSize / 2);
    v.y = (float)(offsetY + row * tileSize + tileSize / 2);
    return v;
}

// draws a picture stretched into a box
void DrawImageInBox(Texture2D tex, Rectangle box)
{
    Rectangle source = {0, 0, (float)tex.width, (float)tex.height};
    DrawTexturePro(tex, source, box, Vector2{0, 0}, 0, WHITE);
}

// draws a button and returns true when it is clicked
bool DrawButton(Rectangle rect, const char *text)
{
    bool hover = CheckCollisionPointRec(GetMousePosition(), rect);

    if (hover)
        DrawRectangleRec(rect, Color{80, 150, 210, 255});
    else
        DrawRectangleRec(rect, Color{50, 95, 145, 255});
    DrawRectangleLinesEx(rect, 2, RAYWHITE);

    int fontSize = 24;
    int textWidth = MeasureText(text, fontSize);
    DrawText(text, (int)(rect.x + rect.width / 2 - textWidth / 2),
             (int)(rect.y + rect.height / 2 - fontSize / 2), fontSize, RAYWHITE);

    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

// text box for the login screen. passwords are drawn as stars
void DrawInputBox(Rectangle rect, std::string text, bool selected, bool hidden)
{
    if (selected)
    {
        DrawRectangleRec(rect, Color{40, 40, 60, 255});
        DrawRectangleLinesEx(rect, 2, SKYBLUE);
    }
    else
    {
        DrawRectangleRec(rect, Color{30, 30, 30, 255});
        DrawRectangleLinesEx(rect, 2, GRAY);
    }

    if (hidden)
        text = std::string(text.size(), '*');
    DrawText(text.c_str(), (int)(rect.x + 10), (int)(rect.y + rect.height / 2 - 10), 20, WHITE);
}

// ------------------------------------------------------------------
// Solvers: BFS, DFS, Dijkstra and A*.
// The base class times the search, each child class only says how it searches.

struct SolveResult {
    bool found;
    int pathLength;
    int visited;            // cells the solver actually looked at
    double microseconds;
    std::vector<int> path;

    SolveResult()
    {
        found = false;
        pathLength = 0;
        visited = 0;
        microseconds = 0;
    }
};

class Solver {
public:
    virtual ~Solver() {}
    virtual const char *Name() = 0;

    // runs the search and times it
    SolveResult Solve(const Graph &g, int start, int goal)
    {
        SolveResult result;
        if (start < 0 || goal < 0 || start >= g.vertex_count || goal >= g.vertex_count)
            return result;

        double before = GetTime();
        Run(g, start, goal, result);
        double after = GetTime();

        result.microseconds = (after - before) * 1000000.0;
        return result;
    }

protected:
    virtual void Run(const Graph &g, int start, int goal, SolveResult &result) = 0;

    // follows the "came from" list back from the goal to make the path
    void BuildPath(std::vector<int> &cameFrom, int start, int goal, SolveResult &result)
    {
        std::vector<int> backwards;
        int v = goal;
        while (v != -1)
        {
            backwards.push_back(v);
            if (v == start)
                break;
            v = cameFrom[v];
        }
        for (int i = (int)backwards.size() - 1; i >= 0; i--)
            result.path.push_back(backwards[i]);

        result.found = true;
        result.pathLength = (int)result.path.size() - 1;
    }
};

// BFS with a queue. always finds a shortest route
class BfsSolver : public Solver {
public:
    const char *Name() { return "BFS"; }

protected:
    void Run(const Graph &g, int start, int goal, SolveResult &result)
    {
        std::vector<int> cameFrom(g.vertex_count, -1);
        std::vector<bool> seen(g.vertex_count, false);
        Queue todo;
        int v;

        if (!queue_init(&todo, g.vertex_count))   // each cell is added once
            return;

        seen[start] = true;
        queue_push(&todo, start);

        while (queue_pop(&todo, &v))
        {
            result.visited++;
            if (v == goal)
                break;

            for (Edge *e = g.adjacency[v]; e != NULL; e = e->next)
            {
                if (!seen[e->to])
                {
                    seen[e->to] = true;
                    cameFrom[e->to] = v;
                    queue_push(&todo, e->to);
                }
            }
        }
        queue_free(&todo);

        if (seen[goal])
            BuildPath(cameFrom, start, goal, result);
    }
};

// DFS with a stack. finds a route, but not always the shortest
class DfsSolver : public Solver {
public:
    const char *Name() { return "DFS"; }

protected:
    void Run(const Graph &g, int start, int goal, SolveResult &result)
    {
        std::vector<int> cameFrom(g.vertex_count, -1);
        std::vector<bool> seen(g.vertex_count, false);
        Stack todo;
        int v;

        if (!stack_init(&todo, g.vertex_count))   // each cell is added once
            return;

        seen[start] = true;
        stack_push(&todo, start);

        while (stack_pop(&todo, &v))
        {
            result.visited++;
            if (v == goal)
                break;

            for (Edge *e = g.adjacency[v]; e != NULL; e = e->next)
            {
                if (!seen[e->to])
                {
                    seen[e->to] = true;
                    cameFrom[e->to] = v;
                    stack_push(&todo, e->to);
                }
            }
        }
        stack_free(&todo);

        if (seen[goal])
            BuildPath(cameFrom, start, goal, result);
    }
};

// Dijkstra with a min-heap. same routes as BFS here, because every move costs 1
class DijkstraSolver : public Solver {
public:
    const char *Name() { return "Dijkstra"; }

protected:
    // guess of the moves left to the goal. Dijkstra doesn't guess
    virtual int Guess(const Graph &g, int v, int goal)
    {
        (void)g;
        (void)v;
        (void)goal;
        return 0;
    }

    void Run(const Graph &g, int start, int goal, SolveResult &result)
    {
        int unreached = g.vertex_count + 1;   // longer than any real route
        std::vector<int> cameFrom(g.vertex_count, -1);
        std::vector<int> distance(g.vertex_count, unreached);
        std::vector<bool> done(g.vertex_count, false);
        Heap heap;
        int v;

        // a cell is pushed each time a shorter way to it is found (at most 4 per cell)
        if (!heap_init(&heap, 4 * g.vertex_count + 1))
            return;

        distance[start] = 0;
        heap_push(&heap, Guess(g, start, goal), start);

        while (heap_pop(&heap, &v))
        {
            if (done[v])
                continue;   // an old, longer entry for v
            done[v] = true;
            result.visited++;
            if (v == goal)
                break;

            for (Edge *e = g.adjacency[v]; e != NULL; e = e->next)
            {
                int newDistance = distance[v] + 1;
                if (newDistance < distance[e->to])
                {
                    distance[e->to] = newDistance;
                    cameFrom[e->to] = v;
                    heap_push(&heap, newDistance + Guess(g, e->to, goal), e->to);
                }
            }
        }
        heap_free(&heap);

        if (distance[goal] != unreached)
            BuildPath(cameFrom, start, goal, result);
    }
};

// A* is Dijkstra plus a guess of the distance left, so it usually looks at fewer cells
class AStarSolver : public DijkstraSolver {
public:
    const char *Name() { return "A*"; }

protected:
    // rows + columns to the goal. never too big, so A* still finds a shortest route
    int Guess(const Graph &g, int v, int goal)
    {
        return abs(g.vertex_row[v] - g.vertex_row[goal]) +
               abs(g.vertex_col[v] - g.vertex_col[goal]);
    }
};

// ------------------------------------------------------------------
class Maze {
public:
    int rows;
    int cols;
    int fruitCount;
    bool winnable;
    int grid[MAX_CELLS];   // 1 = wall, 0 = path
    Graph graph;
    std::vector<int> fruitVertices;

    Maze()
    {
        rows = 0;
        cols = 0;
        fruitCount = 0;
        winnable = false;
        graph_init(&graph, 1, 1);
    }

    ~Maze()
    {
        graph_free(&graph);
    }

    // makes the maze for this seed and places its fruit. returns false if it
    // could not be built. winnable says if the stamina and fruit can reach the exit
    bool Generate(Difficulty difficulty, unsigned int seed)
    {
        Level level = get_level(difficulty);
        rows = level.rows;
        cols = level.cols;
        fruitCount = level.fruit_count;
        winnable = false;
        fruitVertices.clear();

        if (!generate_maze(grid, rows, cols, level.loop_percent, seed))
            return false;
        if (!build_graph(&graph, grid, rows, cols))
            return false;

        // the same seed always gives the same fruit, so the planner and the game agree
        int fruits[MAX_FRUITS];
        int path[MAX_CELLS];
        int count = place_fruits(&graph, StartVertex(), GoalVertex(), fruitCount, seed, fruits);
        for (int i = 0; i < count; i++)
            fruitVertices.push_back(fruits[i]);

        winnable = plan_route(&graph, StartVertex(), GoalVertex(), fruits, count, STAMINA_MAX, path) > 0;
        return true;
    }

    bool IsWall(int row, int col) { return grid[row * cols + col] == 1; }

    // start is always (1,1) and the exit is the opposite corner
    int StartRow() { return 1; }
    int StartCol() { return 1; }
    int GoalRow() { return rows - 2; }
    int GoalCol() { return cols - 2; }

    int VertexAt(int row, int col) { return vertex_at(&graph, row, col); }
    int StartVertex() { return VertexAt(StartRow(), StartCol()); }
    int GoalVertex() { return VertexAt(GoalRow(), GoalCol()); }
};

// ------------------------------------------------------------------
class Player {
public:
    int row;
    int col;
    int stamina;
    int moves;

    Player()
    {
        row = 0;
        col = 0;
        stamina = STAMINA_MAX;
        moves = 0;
    }

    void Reset(Maze &maze)
    {
        row = maze.StartRow();
        col = maze.StartCol();
        stamina = STAMINA_MAX;
        moves = 0;
    }

    void TryMove(int dRow, int dCol, Maze &maze)
    {
        int nextRow = row + dRow;
        int nextCol = col + dCol;

        if (nextRow < 0 || nextRow >= maze.rows || nextCol < 0 || nextCol >= maze.cols)
            return;
        if (maze.IsWall(nextRow, nextCol))
            return;
        if (stamina <= 0)
            return;

        row = nextRow;
        col = nextCol;
        stamina = stamina_after_move(stamina);
        moves++;
    }
};

// ------------------------------------------------------------------
class Fruit {
public:
    int row;
    int col;
    bool collected;
    Texture2D icon;

    Fruit(int r, int c, Texture2D picture)
    {
        row = r;
        col = c;
        collected = false;
        icon = picture;
    }

    void Draw(int offsetX, int offsetY, int tileSize)
    {
        if (collected)
            return;

        Vector2 center = CellCenter(row, col, offsetX, offsetY, tileSize);
        float size = tileSize * 0.8f;
        Rectangle box = {center.x - size / 2, center.y - size / 2, size, size};

        if (icon.id != 0)
            DrawImageInBox(icon, box);
        else
            DrawCircleV(center, size / 3, ORANGE);   // picture not found
    }

    void TryCollect(Player &player)
    {
        if (!collected && player.row == row && player.col == col)
        {
            collected = true;
            player.stamina = stamina_after_fruit(player.stamina);
        }
    }
};

// ------------------------------------------------------------------
class Game {
public:
    Maze maze;
    Player player;
    std::vector<Fruit> fruits;

    Texture2D background;
    Texture2D bush;
    Texture2D playerPicture;
    Texture2D fruitPictures[4];

    int screen;
    int tileSize;
    int offsetX;
    int offsetY;
    Difficulty difficulty;
    bool adaptive;
    bool won;
    bool lost;
    bool resultSaved;
    bool mazeFailed;
    bool quit;
    std::string ratingText;

    // login screen
    Rectangle loginPanel;
    Rectangle signupTab;
    Rectangle loginTab;
    Rectangle userBox;
    Rectangle passBox;
    Rectangle submitButton;
    std::string username;
    std::string password;
    std::string message;
    std::string currentUser;
    double rating;
    int field;
    bool signupMode;

    // hint (H key)
    std::vector<int> hintPath;
    std::string hintMessage;

    // solver comparison (C key)
    bool showSolvers;
    SolveResult solverResults[SOLVER_COUNT];
    const char *solverNames[SOLVER_COUNT];
    Color solverColors[SOLVER_COUNT];

    // leaderboard
    LeaderboardRow board[LEADERBOARD_SIZE];
    int boardCount;
    std::string boardMessage;

    Game()
    {
        InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "MazeQuest - C and C++ Hybrid");
        SetTargetFPS(60);
        SetRandomSeed((unsigned int)time(NULL));

        background = LoadTexture("assets/game_background.png");
        bush = LoadTexture("assets/bush.png");
        playerPicture = LoadTexture("assets/player.png");
        fruitPictures[0] = LoadTexture("assets/apple.png");
        fruitPictures[1] = LoadTexture("assets/banana.png");
        fruitPictures[2] = LoadTexture("assets/grapes.png");
        fruitPictures[3] = LoadTexture("assets/strawberry.png");

        screen = SCREEN_LOGIN;
        tileSize = 30;
        offsetX = 0;
        offsetY = 0;
        difficulty = MEDIUM;
        adaptive = false;
        won = false;
        lost = false;
        resultSaved = false;
        mazeFailed = false;
        quit = false;

        loginPanel = Rectangle{420, 160, 440, 400};
        signupTab = Rectangle{420, 160, 220, 50};
        loginTab = Rectangle{640, 160, 220, 50};
        userBox = Rectangle{450, 260, 380, 45};
        passBox = Rectangle{450, 330, 380, 45};
        submitButton = Rectangle{450, 400, 380, 50};
        rating = START_RATING;
        field = FIELD_NONE;
        signupMode = false;

        showSolvers = false;
        for (int i = 0; i < SOLVER_COUNT; i++)
            solverNames[i] = "";
        solverColors[0] = SKYBLUE;
        solverColors[1] = ORANGE;
        solverColors[2] = MAGENTA;
        solverColors[3] = WHITE;

        boardCount = 0;
    }

    ~Game()
    {
        for (int i = 0; i < 4; i++)
            UnloadTexture(fruitPictures[i]);
        UnloadTexture(bush);
        UnloadTexture(background);
        UnloadTexture(playerPicture);
        CloseWindow();
    }

    void Run()
    {
        while (!WindowShouldClose() && !quit)
        {
            Update();
            Draw();
        }
    }

    // ---------------- update ----------------

    void Update()
    {
        if (screen == SCREEN_LOGIN)
        {
            UpdateLogin();
            return;
        }
        if (screen == SCREEN_MENU || screen == SCREEN_LEADERBOARD)
            return;

        if (IsKeyPressed(KEY_C))
        {
            if (showSolvers)
                showSolvers = false;
            else
                RunSolvers();
        }

        if (won || lost)
        {
            if (IsKeyPressed(KEY_ENTER))
                screen = SCREEN_MENU;
            return;
        }

        int movesBefore = player.moves;

        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
            player.TryMove(-1, 0, maze);
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
            player.TryMove(1, 0, maze);
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))
            player.TryMove(0, -1, maze);
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
            player.TryMove(0, 1, maze);

        if (player.moves != movesBefore)
            hintPath.clear();   // the hint is out of date once the player moves

        for (int i = 0; i < (int)fruits.size(); i++)
            fruits[i].TryCollect(player);

        won = (player.row == maze.GoalRow() && player.col == maze.GoalCol());
        lost = (player.stamina == 0 && !won);

        if (IsKeyPressed(KEY_H) && !won && !lost)
            ShowHint();

        if ((won || lost) && !resultSaved)
            SaveResult();
    }

    // typing, clicks and Enter on the login screen
    void UpdateLogin()
    {
        Vector2 mouse = GetMousePosition();

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            if (CheckCollisionPointRec(mouse, signupTab))
            {
                signupMode = true;
                message.clear();
            }
            else if (CheckCollisionPointRec(mouse, loginTab))
            {
                signupMode = false;
                message.clear();
            }

            if (CheckCollisionPointRec(mouse, userBox))
                field = FIELD_USER;
            else if (CheckCollisionPointRec(mouse, passBox))
                field = FIELD_PASS;
            else
                field = FIELD_NONE;
        }

        if (IsKeyPressed(KEY_TAB))
        {
            if (field == FIELD_USER)
                field = FIELD_PASS;
            else
                field = FIELD_USER;
        }

        std::string *text = NULL;
        if (field == FIELD_USER)
            text = &username;
        if (field == FIELD_PASS)
            text = &password;

        if (text != NULL)
        {
            int letter = GetCharPressed();
            while (letter > 0)
            {
                if (letter >= 32 && letter <= 125 && (int)text->size() < MAX_LEN - 1)
                    text->push_back((char)letter);
                letter = GetCharPressed();
            }

            if (IsKeyPressed(KEY_BACKSPACE) && !text->empty())
                text->pop_back();
        }

        if (IsKeyPressed(KEY_ENTER))
            SubmitLogin();
    }

    // signs up or logs in, depending on the selected tab
    void SubmitLogin()
    {
        char text[128];

        if (signupMode)
        {
            if (signup_user(username.c_str(), password.c_str(), text, sizeof(text)))
            {
                signupMode = false;
                password.clear();
            }
        }
        else if (login_user(username.c_str(), password.c_str(), text, sizeof(text)))
        {
            currentUser = username;
            rating = START_RATING;
            get_rating(currentUser.c_str(), &rating);
            password.clear();
            message.clear();
            screen = SCREEN_MENU;
            return;
        }

        message = text;
    }

    // runs BFS, DFS, Dijkstra and A* from the start of this maze to its exit
    void RunSolvers()
    {
        BfsSolver bfs;
        DfsSolver dfs;
        DijkstraSolver dijkstra;
        AStarSolver astar;
        Solver *solvers[SOLVER_COUNT] = {&bfs, &dfs, &dijkstra, &astar};

        for (int i = 0; i < SOLVER_COUNT; i++)
        {
            solverNames[i] = solvers[i]->Name();
            solverResults[i] = solvers[i]->Solve(maze.graph, maze.StartVertex(), maze.GoalVertex());
        }
        showSolvers = true;
    }

    // best route from where the player stands, with their stamina and the fruit left
    void ShowHint()
    {
        int fruitsLeft[MAX_FRUITS];
        int count = 0;
        for (int i = 0; i < (int)fruits.size(); i++)
        {
            if (!fruits[i].collected)
            {
                fruitsLeft[count] = maze.VertexAt(fruits[i].row, fruits[i].col);
                count++;
            }
        }

        int path[MAX_CELLS];
        int length = plan_route(&maze.graph, maze.VertexAt(player.row, player.col),
                                maze.GoalVertex(), fruitsLeft, count, player.stamina, path);

        hintPath.clear();
        if (length > 0)
        {
            for (int i = 1; i < length; i++)   // skip the cell the player is on
                hintPath.push_back(path[i]);
            hintMessage = "Follow the dots.";
        }
        else
        {
            hintMessage = "No winning route from here.";
        }
    }

    // saves the finished game once and shows the new rating
    void SaveResult()
    {
        double before = rating;
        resultSaved = true;

        if (save_game(currentUser.c_str(), (int)difficulty, maze.rows, maze.cols, won,
                      player.moves, player.stamina, &rating))
        {
            char text[64];
            snprintf(text, sizeof(text), "Rating %.0f -> %.0f (%+.0f)", before, rating, rating - before);
            ratingText = text;
        }
        else
        {
            ratingText = "Could not save this game.";
        }
    }

    void OpenLeaderboard()
    {
        boardCount = get_leaderboard(board, LEADERBOARD_SIZE);
        if (boardCount < 0)
            boardCount = 0;
        boardMessage.clear();
        screen = SCREEN_LEADERBOARD;
    }

    // tries random mazes until one can be won (hard mazes often can't)
    bool MakeWinnableMaze(Difficulty diff)
    {
        for (int attempt = 0; attempt < 200; attempt++)
        {
            unsigned int seed = (unsigned int)GetRandomValue(1, 1000000000);
            if (maze.Generate(diff, seed) && maze.winnable)
                return true;
        }
        return false;
    }

    void StartGame(Difficulty diff)
    {
        difficulty = diff;
        adaptive = false;
        mazeFailed = !MakeWinnableMaze(diff);
        if (mazeFailed)
            return;
        BeginPlaying();
    }

    // lets the adaptive chooser pick a maze that fits the player's rating
    void StartAdaptive()
    {
        MazeChoice choice;
        unsigned int seed = (unsigned int)GetRandomValue(1, 1000000000);

        // the same difficulty + seed gives back exactly the maze that was scored
        mazeFailed = true;
        if (choose_adaptive_maze(rating, seed, &choice) &&
            maze.Generate(choice.difficulty, choice.seed) && maze.winnable)
            mazeFailed = false;
        if (mazeFailed)
            return;

        difficulty = choice.difficulty;
        adaptive = true;
        BeginPlaying();
    }

    // fits the maze on the screen and starts a new game on it
    void BeginPlaying()
    {
        tileSize = (SCREEN_HEIGHT - 40) / maze.rows;
        offsetX = (SCREEN_WIDTH - maze.cols * tileSize) / 2;
        offsetY = (SCREEN_HEIGHT - maze.rows * tileSize) / 2;

        player.Reset(maze);

        fruits.clear();
        for (int i = 0; i < (int)maze.fruitVertices.size(); i++)
        {
            int v = maze.fruitVertices[i];
            fruits.push_back(Fruit(maze.graph.vertex_row[v], maze.graph.vertex_col[v], fruitPictures[i % 4]));
        }

        hintPath.clear();
        hintMessage.clear();
        showSolvers = false;
        won = false;
        lost = false;
        resultSaved = false;
        ratingText.clear();
        screen = SCREEN_PLAYING;
    }

    // ---------------- draw ----------------

    void Draw()
    {
        BeginDrawing();
        ClearBackground(BLACK);

        if (background.id != 0)
            DrawImageInBox(background, Rectangle{0, 0, SCREEN_WIDTH, SCREEN_HEIGHT});
        else
            ClearBackground(Color{22, 34, 29, 255});

        if (screen == SCREEN_LOGIN)
        {
            DrawLogin();
        }
        else if (screen == SCREEN_MENU)
        {
            DrawMenu();
        }
        else if (screen == SCREEN_LEADERBOARD)
        {
            DrawLeaderboard();
        }
        else
        {
            DrawMaze();
            DrawHud();
            if (showSolvers)
                DrawSolverPanel();

            if (won)
                DrawText("YOU ESCAPED!", 515, 330, 40, GREEN);
            if (lost)
                DrawText("STAMINA EMPTY", 500, 330, 40, RED);
            if (won || lost)
            {
                DrawText("Press ENTER for menu", 520, 380, 24, RAYWHITE);
                DrawText(ratingText.c_str(), 520, 420, 22, YELLOW);
            }
        }

        EndDrawing();
    }

    void DrawLogin()
    {
        Color tabColor = {40, 40, 50, 255};

        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Color{0, 0, 0, 120});
        DrawText("MazeQuest", 505, 90, 54, RAYWHITE);

        DrawRectangleRec(loginPanel, Color{25, 25, 35, 180});
        DrawRectangleLinesEx(loginPanel, 2, GRAY);

        DrawRectangleRec(signupTab, signupMode ? SKYBLUE : tabColor);
        DrawRectangleRec(loginTab, signupMode ? tabColor : SKYBLUE);
        DrawText("SIGN UP", (int)signupTab.x + 20, (int)signupTab.y + 15, 20, WHITE);
        DrawText("LOG IN", (int)loginTab.x + 30, (int)loginTab.y + 15, 20, WHITE);

        DrawText("Username:", (int)userBox.x, (int)userBox.y - 22, 16, LIGHTGRAY);
        DrawInputBox(userBox, username, field == FIELD_USER, false);
        DrawText("Password:", (int)passBox.x, (int)passBox.y - 22, 16, LIGHTGRAY);
        DrawInputBox(passBox, password, field == FIELD_PASS, true);

        if (DrawButton(submitButton, signupMode ? "CREATE ACCOUNT" : "LOG IN"))
            SubmitLogin();

        if (!message.empty())
            DrawText(message.c_str(), (int)loginPanel.x + 30, (int)submitButton.y + 70, 18, YELLOW);
    }

    void DrawMenu()
    {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Color{0, 0, 0, 120});
        DrawText("MazeQuest", 505, 120, 54, RAYWHITE);
        DrawText(TextFormat("Logged in as %s   Rating: %.0f", currentUser.c_str(), rating), 460, 195, 22, RAYWHITE);

        if (DrawButton(Rectangle{520, 235, 240, 52}, "EASY"))
            StartGame(EASY);
        if (DrawButton(Rectangle{520, 297, 240, 52}, "MEDIUM"))
            StartGame(MEDIUM);
        if (DrawButton(Rectangle{520, 359, 240, 52}, "HARD"))
            StartGame(HARD);
        if (DrawButton(Rectangle{520, 421, 240, 52}, "ADAPTIVE"))
            StartAdaptive();
        if (DrawButton(Rectangle{520, 483, 240, 52}, "LEADERBOARD"))
            OpenLeaderboard();
        if (DrawButton(Rectangle{520, 545, 240, 52}, "LOG OUT"))
        {
            currentUser.clear();
            rating = START_RATING;
            username.clear();
            password.clear();
            message.clear();
            field = FIELD_NONE;
            screen = SCREEN_LOGIN;
        }
        if (DrawButton(Rectangle{520, 607, 240, 52}, "QUIT"))
            quit = true;

        if (mazeFailed)
            DrawText("Could not make a valid maze. Try again.", 460, 672, 20, RED);
    }

    void DrawLeaderboard()
    {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Color{0, 0, 0, 120});
        DrawRectangle(300, 80, 680, 560, Color{25, 25, 35, 200});
        DrawRectangleLines(300, 80, 680, 560, GRAY);
        DrawText("LEADERBOARD", 520, 100, 32, RAYWHITE);

        DrawText("#", 330, 155, 18, LIGHTGRAY);
        DrawText("Player", 380, 155, 18, LIGHTGRAY);
        DrawText("Rating", 650, 155, 18, LIGHTGRAY);
        DrawText("Games", 760, 155, 18, LIGHTGRAY);
        DrawText("Wins", 860, 155, 18, LIGHTGRAY);

        for (int i = 0; i < boardCount; i++)
        {
            int y = 188 + i * 30;
            Color color = RAYWHITE;
            if (currentUser == board[i].username)   // you are shown in yellow
                color = YELLOW;

            DrawText(TextFormat("%d", i + 1), 330, y, 20, color);
            DrawText(board[i].username, 380, y, 20, color);
            DrawText(TextFormat("%.0f", board[i].rating), 650, y, 20, color);
            DrawText(TextFormat("%d", board[i].games), 760, y, 20, color);
            DrawText(TextFormat("%d", board[i].wins), 860, y, 20, color);
        }

        if (boardCount == 0)
            DrawText("No players yet.", 380, 190, 20, LIGHTGRAY);

        if (DrawButton(Rectangle{330, 520, 200, 50}, "BACK"))
            screen = SCREEN_MENU;

        if (DrawButton(Rectangle{560, 520, 250, 50}, "EXPORT CSV"))
        {
            int count = export_csv("play_log.csv");
            if (count >= 0)
                boardMessage = "Saved " + std::to_string(count) + " games to play_log.csv";
            else
                boardMessage = "Could not write play_log.csv";
        }

        if (!boardMessage.empty())
            DrawText(boardMessage.c_str(), 330, 590, 18, YELLOW);
    }

    void DrawMaze()
    {
        for (int row = 0; row < maze.rows; row++)
        {
            for (int col = 0; col < maze.cols; col++)
            {
                Rectangle tile = {(float)(offsetX + col * tileSize), (float)(offsetY + row * tileSize),
                                  (float)tileSize, (float)tileSize};

                if (!maze.IsWall(row, col))
                    DrawRectangleRec(tile, Color{12, 14, 18, 115});
                else if (bush.id != 0)
                    DrawImageInBox(bush, tile);
                else
                    DrawRectangleRec(tile, DARKGREEN);
            }
        }

        for (int i = 0; i < (int)fruits.size(); i++)
            fruits[i].Draw(offsetX, offsetY, tileSize);

        // each solver's route as small dots, moved a bit so they don't cover each other
        if (showSolvers)
        {
            int shift = tileSize / 5;
            float dotSize = tileSize / 10.0f;
            if (dotSize < 2.0f)
                dotSize = 2.0f;

            for (int i = 0; i < SOLVER_COUNT; i++)
            {
                float dx = (float)shift;
                float dy = (float)shift;
                if (i % 2 == 0)
                    dx = -dx;
                if (i < 2)
                    dy = -dy;

                for (int k = 0; k < (int)solverResults[i].path.size(); k++)
                {
                    int v = solverResults[i].path[k];
                    Vector2 center = CellCenter(maze.graph.vertex_row[v], maze.graph.vertex_col[v],
                                                offsetX, offsetY, tileSize);
                    DrawCircleV(Vector2{center.x + dx, center.y + dy}, dotSize, solverColors[i]);
                }
            }
        }

        // hint route
        for (int k = 0; k < (int)hintPath.size(); k++)
        {
            int v = hintPath[k];
            Vector2 center = CellCenter(maze.graph.vertex_row[v], maze.graph.vertex_col[v],
                                        offsetX, offsetY, tileSize);
            DrawCircleV(center, tileSize / 6.0f, YELLOW);
        }

        // player
        Vector2 center = CellCenter(player.row, player.col, offsetX, offsetY, tileSize);
        float size = (float)(tileSize - 2);
        if (playerPicture.id != 0)
            DrawImageInBox(playerPicture, Rectangle{center.x - size / 2, center.y - size / 2, size, size});
        else
            DrawCircleV(center, size / 3, SKYBLUE);

        // exit
        DrawCircleV(CellCenter(maze.GoalRow(), maze.GoalCol(), offsetX, offsetY, tileSize),
                    tileSize / 3.0f, LIME);
    }

    void DrawHud()
    {
        int barWidth = (int)(240 * (player.stamina / (float)STAMINA_MAX));
        DrawText("Stamina", 36, 30, 20, RAYWHITE);
        DrawRectangle(36, 58, 240, 16, DARKGRAY);
        DrawRectangle(36, 58, barWidth, 16, GREEN);
        DrawText(TextFormat("%d", player.stamina), 288, 54, 22, RAYWHITE);
        DrawText(TextFormat("Player: %s", currentUser.c_str()), 36, 90, 20, RAYWHITE);
        DrawText("H: hint   C: solvers", 36, 115, 18, LIGHTGRAY);
        if (!hintMessage.empty())
            DrawText(hintMessage.c_str(), 36, 140, 18, YELLOW);
        if (adaptive)
            DrawText(TextFormat("Adaptive maze (%s)", difficultyNames[(int)difficulty]), 36, 165, 18, SKYBLUE);
    }

    // table of solver results on the left of the screen
    void DrawSolverPanel()
    {
        int x = 15;
        int y = 215;

        DrawRectangle(x, y, 275, 190, Color{0, 0, 0, 170});
        DrawRectangleLines(x, y, 275, 190, GRAY);
        DrawText("Solvers, start to exit", x + 10, y + 8, 16, RAYWHITE);
        DrawText("checked", x + 112, y + 34, 14, LIGHTGRAY);
        DrawText("route", x + 178, y + 34, 14, LIGHTGRAY);
        DrawText("us", x + 233, y + 34, 14, LIGHTGRAY);

        for (int i = 0; i < SOLVER_COUNT; i++)
        {
            int row = y + 56 + i * 28;
            DrawRectangle(x + 10, row + 2, 12, 12, solverColors[i]);
            DrawText(solverNames[i], x + 30, row, 16, RAYWHITE);
            DrawText(TextFormat("%d", solverResults[i].visited), x + 112, row, 16, RAYWHITE);
            DrawText(TextFormat("%d", solverResults[i].pathLength), x + 178, row, 16, RAYWHITE);
            DrawText(TextFormat("%.1f", solverResults[i].microseconds), x + 225, row, 16, RAYWHITE);
        }

        DrawText("Press C to hide", x + 10, y + 168, 14, LIGHTGRAY);
    }
};

int main()
{
    Game game;
    game.Run();
    return 0;
}
