# Run from the project folder:
#   make game | make cli | make menu | make dataset | make train | make check-model

CC = cc
CXX = c++
PYTHON = python3
CFLAGS = -O2 -Wall -Wextra -Werror -std=c11 -Iinclude -Imodel
CXXFLAGS = -O2 -Wall -Wextra -Werror -std=c++17 -Iinclude

# Raylib from Homebrew on Mac. On Windows (MSYS2) run:
#   make game RAYLIB_LIBS='-lraylib -lopengl32 -lgdi32 -lwinmm'
ifneq ($(wildcard /opt/homebrew/include/raylib.h),)
RAYLIB_CFLAGS = -I/opt/homebrew/include
RAYLIB_LIBS = -L/opt/homebrew/lib -lraylib
else
RAYLIB_CFLAGS =
RAYLIB_LIBS = -lraylib
endif

HEADERS = $(wildcard include/*.h) model/maze_model.h

# the maze code: C files that the dataset program and the game both use
MAZE_OBJECTS = build/maze_structs.o build/maze_graph.o build/maze_planner.o build/maze_adaptive.o build/maze_generator.o

.PHONY: all game cli menu dataset train check-model
.DELETE_ON_ERROR:

all: build/maze_dataset

build:
	mkdir -p build

build/%.o: src/%.c $(HEADERS) | build
	$(CC) $(CFLAGS) -c $< -o $@

build/maze_dataset: src/maze_dataset.c $(MAZE_OBJECTS) $(HEADERS) | build
	$(CC) $(CFLAGS) src/maze_dataset.c $(MAZE_OBJECTS) -lm -o $@

build/maze_cli: src/maze_generator.c $(HEADERS) | build
	$(CC) $(CFLAGS) -DMAZE_CLI src/maze_generator.c -o $@

# the game is C++ and links the C files compiled above
build/maze_game: src/maze_game.cpp include/maze_solvers.hpp $(MAZE_OBJECTS) build/maze_users.o | build
	$(CXX) $(CXXFLAGS) $(RAYLIB_CFLAGS) src/maze_game.cpp $(MAZE_OBJECTS) build/maze_users.o $(RAYLIB_LIBS) -lsqlite3 -lm -o $@

build/menu: src/menu.c build/maze_users.o | build
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) src/menu.c build/maze_users.o $(RAYLIB_LIBS) -lsqlite3 -lm -o $@

game: build/maze_game
cli: build/maze_cli
menu: build/menu

dataset: build/maze_dataset
	mkdir -p data
	./build/maze_dataset 200 64 1 > data/maze_data.csv.tmp
	mv data/maze_data.csv.tmp data/maze_data.csv

train: dataset
	$(PYTHON) tools/train_model.py data/maze_data.csv --output model
	$(MAKE) check-model

check-model: build/maze_dataset
	$(PYTHON) tools/train_model.py data/maze_data.csv --output model --check ./build/maze_dataset
