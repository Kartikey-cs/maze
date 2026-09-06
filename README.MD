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

- `maze_graph.c` / `maze_graph.h`: C data-structure layer. It builds the maze as a graph with adjacency lists and implements BFS with a queue plus DFS with a stack.
- `maze_game.cpp`: C++ game layer. It uses simple `Maze`, `Player`, `Fruit`, and `Game` classes to show object-oriented programming. It also contains the start menu and game screen flow.
- `menu.c`: separate C login/signup prototype with SQLite. The hybrid gameplay executable has its own simple start menu.

The maze used by the hybrid game is hard-coded in `maze_game.cpp` as a fixed 21 x 21 grid. It is not downloaded or randomly generated.

Fruit pickups use PNG icons from `resources/scrimsy fruit icons/`.

Build the hybrid game with MSYS2 UCRT64. Compile the C data-structure file with `gcc`, then link it with the C++ game classes using `g++`:

```sh
gcc -Wall -Wextra -std=c11 -c maze_graph.c -o maze_graph.o
g++ -Wall -Wextra -std=c++17 maze_game.cpp maze_graph.o -o MazeQuestHybrid.exe -lraylib -lopengl32 -lgdi32 -lwinmm
```

