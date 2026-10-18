## Project layout

```text
src/        C implementations and the C++ game
include/    Shared maze headers
tests/      C tests, Python tests, and the visual maze demo
tools/      Offline model training and verification scripts
vendor/     Third-party SQLite header
data/       Generated training dataset
model/      Exported C model, parameters, and results
assets/     Game backgrounds and sprites
resources/  Fruit icons and character assets
build/      Executables and objects (ignored by Git)
```

Run commands from the repository root. See [START.md](START.md) for
the game, terminal generator, and login menu. Older root-level executables are
preserved under `build/legacy/`.

### Prerequisites
Fruit images thanks to:https://scrimsy.itch.io/fruit-icons-16x16
## Raylib
 Documentation: https://www.raylib.com/
 Cmd: pacman -S mingw-w64-ucrt-x86_64-raylib
Used for:

- Window creation
- Graphics rendering
- Input handling
- Audio
- Textures
- Animations

## Raygui
Raygui.h(https://github.com/raysan5/raygui)
Used for:

- Textboxes
- Buttons
- Sliders
- Checkboxes
- GUI widgets

## C/C++ Hybrid Structure

The playable hybrid build is split the way the proposal describes:

- `src/maze_graph.c` / `include/maze_graph.h`: C data-structure layer. It builds the maze as a graph with adjacency lists and implements BFS with a queue plus DFS with a stack.
- `src/maze_game.cpp`: C++ game layer. It uses simple `Maze`, `Player`, `Fruit`, and `Game` classes to show object-oriented programming. It also contains the start menu and game screen flow.
- `src/maze_auth.c` / `include/maze_auth.h`: C signup and login on SQLite. The game opens on a login screen that uses it, and `src/menu.c` is a standalone login window that shares the same code.

Each game generates a new random maze with `src/maze_generator.c`. The size
(15, 21 or 31 cells wide), number of fruits, and start and goal cells come from
the difficulty chosen in the menu.

Fruit pickups use PNG icons from `resources/scrimsy fruit icons/`.

Build the hybrid game with MSYS2 UCRT64. Compile the C data-structure file with `gcc`, then link it with the C++ game classes using `g++`:

```sh
mkdir -p build
gcc -Wall -Wextra -std=c11 -Iinclude -c src/maze_graph.c -o build/maze_graph.o
gcc -Wall -Wextra -std=c11 -Iinclude -c src/maze_generator.c -o build/maze_generator.o
gcc -Wall -Wextra -std=c11 -Iinclude -Ivendor -c src/maze_auth.c -o build/maze_auth.o
g++ -Wall -Wextra -std=c++17 -Iinclude src/maze_game.cpp build/maze_graph.o build/maze_generator.o build/maze_auth.o -o build/MazeQuestHybrid.exe -lraylib -lopengl32 -lgdi32 -lwinmm -lsqlite3
```

## Maze features, simulated players, and difficulty model

The feature extractor, simulated players, dataset generator, and exported model
scorer are C11. Python is used only for offline training and C/Python verification,
as requested in the assignment. The game and C model have no Python runtime
dependency. This pipeline needs a C compiler, GNU Make, and Python 3.10+ with its
standard library; Raylib, NumPy, and scikit-learn are not needed.

From the repository root (macOS/Linux terminal or MSYS2 on Windows):

```sh
make                  # Build the C dataset generator
make test             # C unit tests, generator tests, Python tests, score parity
make train            # Regenerate the dataset, train/export, and verify C scores
```

`make train` generates 600 mazes: seeds 1–200 at each existing difficulty preset,
with 64 trials per bot per maze (76,800 simulated runs). Outputs are:

- `data/maze_data.csv`: seed, difficulty, fruit/trial counts, seven features,
  per-bot success counts, and the pooled success-rate label.
- `model/maze_model.h`: C weights, training means/scales, and
  `MazeModel_PredictSuccess`, returning an estimated success rate in `[0, 1]`.
- `model/maze_model.json`: the same parameters for Python verification.
- `model/report.json`: held-out errors, baseline parameters, seed groups,
  dataset checksum, and an explicit comparison verdict.

To run a different experiment without overwriting these reference artifacts:

```sh
./build/maze_dataset 100 32 1001 > build/custom.csv
python3 tools/train_model.py build/custom.csv --output build/custom_model
cc -O2 -std=c11 -Ibuild/custom_model src/maze_model_score.c -o build/custom_score
python3 tools/verify_model.py build/custom.csv build/custom_model/maze_model.json ./build/custom_score
```

Generator arguments are `seed_count trials_per_bot first_seed`; all must be
positive, and seeds must fit in 32 bits. Seed zero is excluded because the existing
maze generator treats it as a time-based seed.

### Features and simulation rules

`src/maze_features.c` extracts features from a connected `MazeGraph`: vertex count,
undirected edge count, dead ends (degree 1), junctions (degree >= 3), independent
loops (`edges - vertices + 1`), BFS shortest-path length in **moves**, and that
length divided by Euclidean distance in grid cells. Coincident endpoints have
length 0 and ratio 1. Invalid endpoints, empty graphs, and disconnected graphs
return failure. Feature vector order is fixed by `MazeFeatures_ToVector`.

Both bots share `include/maze_rules.h` with the game: start at 100 stamina, spend 1 per
legal move, and collect each fruit once for +20, capped at 100. Arriving at the
exit with zero stamina succeeds; a fruit reached on the last move can restore
stamina before exhaustion is checked. The random walker chooses a legal neighbor.
The greedy explorer chooses the least-visited neighbor, then the one closest to
the exit by Manhattan distance, with seeded random tie-breaking. Neither bot
looks up the BFS route.

Each maze has one reproducible fruit layout, sampled without replacement from
walkable cells excluding the start and exit. All trials and both bots share that
layout. Fruit counts come from the existing difficulty presets: 5/4/3 for
easy/medium/hard. The label is total successes divided by twice the trials per
bot, so the two policies contribute equally.

### Training and results

The model is a small linear ridge regression with regularization 0.001. Means,
scales, and coefficients are fitted on training seeds only. Seeds divisible by
5 are held out across **all** difficulties: 480 training mazes and 120 test mazes.
The baseline fits the same kind of regression to the single value
`shortest_path + dead_ends`. Both predictions are clipped to `[0, 1]` and compared
using held-out mean squared error. Training always reports whether the model
beats the baseline, including when it does not.

The supplied reference run scored **0.01358164** MSE versus the baseline's
**0.01524337** (about 10.9% lower). C and Python predictions matched within
`1e-12` across all 600 maze vectors and two boundary probes. These scores measure
this mixture of bots under the preset sizes and fruit rules; they have not been
validated as human difficulty estimates or on other maze/fruit configurations.

Use the exported model from C without Python:

```c
#include "maze_features.h"
#include "model/maze_model.h"

/* graph has already been initialized and built from a grid. */
MazeFeatures features;
if (MazeFeatures_Extract(&graph, start_row, start_col, goal_row, goal_col, &features)) {
    double vector[MAZE_FEATURE_COUNT];
    MazeFeatures_ToVector(&features, vector);
    double success_rate = MazeModel_PredictSuccess(vector);
    double difficulty = 1.0 - success_rate;
    /* Use success_rate or difficulty here. */
}
```

Compile the caller with `-Iinclude -I. src/maze_features.c src/maze_graph.c -lm`. The header is small
and needs no training library. The playable game generates mazes using the selected
difficulty; the trained model is available as a separate C scoring API.

