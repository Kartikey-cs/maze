# MazeQuest

A maze game made with C and C++. The maze is generated in C, stored as a graph
(adjacency list), and checked with BFS (queue) and DFS (stack). The game screen
is written in C++ with classes. Players log in, get an Elo-style rating, and can
compare four path-finding algorithms on the maze. A small trained model guesses
how hard a maze is.

## Files

```text
include/maze.h          all structs and function declarations
src/maze_generator.c    makes a random maze (DFS backtracker) + terminal version
src/maze_core.c         graph, BFS, DFS, queue/stack/heap, maze features, the two bots,
                        stamina rules, route planner, adaptive maze chooser
src/maze_users.c        sign up / log in (salted SHA-256), ratings, leaderboard, CSV export
src/maze_game.cpp       the game (C++ classes: Maze, Player, Fruit, Game, and the
                        solver classes BFS, DFS, Dijkstra, A*)
src/menu.c              stand-alone login window (same account code as the game)
src/maze_dataset.c      makes the training data, and --score runs the model
tools/train_model.py    trains the model and checks C gives the same answers
data/maze_data.csv      the training data
model/                  trained model (maze_model.h for C, .json, report.json)
assets/                 pictures used by the game and menu
roadmap/                separate program: Dehradun road map (see roadmap/README.md)
```

## How to run

Run every command from this folder (the one with the `Makefile`), otherwise the
game can't find the pictures in `assets/`.

```sh
make game && ./build/maze_game     # the game
make cli && ./build/maze_cli       # maze in the terminal (no raylib needed)
make menu && ./build/menu          # stand-alone login window
make check-model                   # check the C model matches Python
make train                         # make the data again and retrain the model
```

### Playing

1. The game opens on a login screen. Use **SIGN UP** the first time, then **LOG IN**
   (Tab switches boxes, Enter submits). Accounts are saved in `maze_game.db` in the
   folder you run the game from.
2. Pick **EASY**, **MEDIUM**, **HARD** or **ADAPTIVE**. Adaptive makes several mazes,
   drops the ones that can't be won, and picks one that suits your rating.
3. Move with WASD or the arrow keys. Every move costs 1 stamina, each fruit gives
   +20 (max 100). Reach the green circle before stamina runs out.
4. **H** shows the best route from where you are (yellow dots). **C** compares
   BFS, DFS, Dijkstra and A* on the maze (blue, orange, magenta, white).
5. After the game your rating changes. Press Enter to go back to the menu.
   **LEADERBOARD** shows the top 10 players and can export every game to `play_log.csv`.

Every maze is checked with the stamina planner first, so it can always be won.
Hard mazes are often rejected, so Hard can take a moment to start.

### What you need

| Program | Needs |
| --- | --- |
| Game | C and C++ compiler, make, raylib, SQLite (`libsqlite3-dev` on Linux) |
| Terminal maze | C compiler, make |
| Login window | C compiler, make, raylib, SQLite |
| Model training | C compiler, make, Python 3.10+ (no extra packages) |

Raylib from Homebrew (`/opt/homebrew`) is found automatically on Mac.
On Windows use the MSYS2 UCRT64 terminal (with the raylib and sqlite3 packages):

```sh
make game RAYLIB_LIBS='-lraylib -lopengl32 -lgdi32 -lwinmm'
./build/maze_game.exe
```

If `python3` is called `python` on your computer: `make train PYTHON=python`.

## How it works

**Maze:** `generate_maze()` starts with all walls and does a DFS from cell (1,1),
breaking walls to random unvisited neighbours. Easy and medium mazes then break
some extra walls so there are loops. `build_graph()` turns every open cell into a
vertex and joins neighbours with edges (adjacency list).

| Difficulty | Size | Loops | Fruits |
| --- | --- | --- | --- |
| Easy | 15 x 15 | 15% | 5 |
| Medium | 21 x 21 | 5% | 4 |
| Hard | 31 x 31 | 0% | 3 |

**Planner (`plan_route`):** a BFS where each state is (cell, stamina left, which
fruits are eaten). The first time it reaches the exit is the shortest route that
can actually be won. The game uses it to reject impossible mazes and for the H hint.

**Solvers:** `Solver` is a base class; `BfsSolver`, `DfsSolver`, `DijkstraSolver`
and `AStarSolver` only change how they search (queue, stack, min-heap, and heap
plus the Manhattan distance guess). The table shows cells checked, route length
and time in microseconds.

**Accounts:** passwords are saved as a random salt plus SHA-256 repeated 10000
times. Old accounts with the earlier unsalted hash still log in and are upgraded
when they do. All queries use `?` placeholders, so usernames can't inject SQL.

**Ratings:** Elo with K = 32. Easy, medium and hard mazes count as 800, 1000 and
1200. Saving a game and updating the rating happen in one transaction.

## Difficulty model

Two bots play each maze with the same stamina rules as the player:

- **random bot**: goes to any neighbour
- **greedy bot**: goes to the neighbour it visited least, and if tied, the one
  closest to the exit (Manhattan distance)

`make train` makes 600 mazes (seeds 1-200, all 3 difficulties), runs each bot
64 times per maze, and saves the success rate with 7 features of the maze:
vertices, edges, dead ends, junctions, loops, shortest path length (moves),
and detour ratio (shortest path / straight line distance).

`tools/train_model.py` fits a ridge regression (only the Python standard
library). Every 5th seed is kept for testing, with all its difficulties, so the
same maze seed is never in both train and test. We compare it with a baseline
that only uses `shortest_path + dead_ends`.

**Result:** test MSE **0.01358** for the model vs **0.01524** for the baseline
(about 10.9% better). The C version of the model (`model/maze_model.h`) gives
the same answers as Python within 1e-12 on all 600 mazes. This only measures
these two bots, not real players. The adaptive mode uses this model to sort
mazes from easiest to hardest.

To try a different experiment without changing the saved files:

```sh
./build/maze_dataset 100 32 1001 > build/custom.csv
python3 tools/train_model.py build/custom.csv --output build/custom_model
```

## Common problems

- **Pictures missing:** run `./build/maze_game` from this folder, not from inside `build/`.
- **`raylib.h` not found:** install raylib, or build with
  `make game RAYLIB_CFLAGS=-I/path/to/include RAYLIB_LIBS='-L/path/to/lib -lraylib'`.
- **`sqlite3.h` not found:** install SQLite's development files
  (`libsqlite3-dev` on Linux, the sqlite3 package in MSYS2).

## Credits

- Fruit icons: https://scrimsy.itch.io/fruit-icons-16x16
- Raylib: https://www.raylib.com/
