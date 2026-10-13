# Starting MazeQuest

Open a terminal in the project folder containing `Makefile`. Run all commands
below from that folder so the game can find `assets/` and `resources/`.

## Quick start: play the game

```sh
make game
./build/maze_game
```

Click **START GAME**, then use **WASD** or the **arrow keys** to move. Reach the
exit before stamina runs out. Each move costs 1 stamina; each fruit restores 20,
up to a maximum of 100. Close the window to exit.

The playable game uses a fixed maze. The separate training pipeline generates
mazes and scores their difficulty for simulated players.

## Requirements

| What you want to run | Requirements |
| --- | --- |
| Main game | C11 compiler, C++17 compiler, GNU Make, Raylib |
| Terminal maze generator | C11 compiler, GNU Make |
| Login/signup prototype | C11 compiler, GNU Make, Raylib, SQLite library |
| Tests and model training | C11 compiler, GNU Make, Python 3.10+ |

Python is used for offline training and verification. The game does not need
Python, and the training scripts use only the Python standard library.

You can check the available tools with:

```sh
cc --version
c++ --version
make --version
python3 --version
```

The Makefile detects Raylib installed under `/opt/homebrew` on this Mac. For a
different installation, override `RAYLIB_CFLAGS` and `RAYLIB_LIBS` with the actual
include and library locations.

On Windows, use the MSYS2 UCRT64 terminal with the required compiler, Make, and
Raylib packages installed:

```sh
make game RAYLIB_LIBS='-lraylib -lopengl32 -lgdi32 -lwinmm'
./build/maze_game.exe
```

## Other programs

### Terminal maze generator

```sh
make cli
./build/maze_cli
```

Choose **1** for easy, **2** for medium, **3** for hard, or **0** to exit. This
prints a generated maze in the terminal and does not require Raylib.

### Login/signup prototype

```sh
make menu
./build/menu
```

This opens the separate login/signup interface. It is a prototype and is not
connected to the main game's start screen.

## Tests and model training

Run the full test suite:

```sh
make test
```

This runs the C feature and bot tests, checks generated mazes, runs the Python
training tests, and verifies that C and Python model predictions match.

Regenerate the dataset and train the model:

```sh
make train
```

This generates 600 mazes using seeds 1–200 across three difficulty presets,
runs 64 trials per bot per maze, holds out whole seeds for testing, compares the
model against the baseline, and exports the weights to C.

| Command | What it does |
| --- | --- |
| `make` | Builds the C dataset generator; does not launch the game |
| `make game` | Builds the main game |
| `make cli` | Builds the terminal maze generator |
| `make menu` | Builds the login/signup prototype |
| `make test` | Runs all automated tests and score verification |
| `make dataset` | Regenerates `data/maze_data.csv` |
| `make train` | Regenerates data, trains/exports the model, verifies scores |
| `make check-model` | Compares exported C scores with Python predictions |

## Where files are saved

- `build/`: compiled programs and object files.
- `build/legacy/`: preserved executables from before the folder cleanup.
- `data/maze_data.csv`: generated maze features and bot results.
- `model/maze_model.h`: exported model that C can use without Python.
- `model/maze_model.json`: model parameters for verification.
- `model/report.json`: held-out test errors and baseline comparison.
- `tools/`: offline training and verification scripts.

`make train` replaces the reference dataset and model files. See
[README.MD](README.MD) for commands to run a separate custom experiment.

## Common startup issues

- **Images are missing:** run the executable from the project root, using
  `./build/maze_game`, rather than changing into `build/` first.
- **`raylib.h` is missing or linking fails:** check that Raylib is installed and
  supply the appropriate `RAYLIB_CFLAGS` and `RAYLIB_LIBS` when building.
- **`make` is missing:** use a terminal with GNU Make available; on Windows this
  should be the MSYS2 UCRT64 terminal.
- **`python3` is missing:** use your installed Python 3.10+ command, for example
  `make test PYTHON=python` or `make train PYTHON=python`.

All commands above operate locally. They do not commit or push to Git.
