CC ?= cc
CXX ?= c++
PYTHON ?= python3
CFLAGS ?= -O2 -Wall -Wextra -Werror -std=c11
CXXFLAGS ?= -O2 -Wall -Wextra -Werror -std=c++17
CPPFLAGS ?=
LDLIBS ?= -lm
HEADERS = $(wildcard include/*.h) Makefile

SQLITE_LIBS ?= -lsqlite3

# Homebrew installs Raylib here; other platforms can override these variables.
ifneq ($(wildcard /opt/homebrew/include/raylib.h),)
RAYLIB_CFLAGS ?= -I/opt/homebrew/include
RAYLIB_LIBS ?= -L/opt/homebrew/lib -lraylib
else
RAYLIB_CFLAGS ?=
RAYLIB_LIBS ?= -lraylib
endif

.PHONY: all test dataset train check-model game cli menu
.DELETE_ON_ERROR:
all: build/maze_dataset

build:
	mkdir -p build

build/maze_dataset: src/maze_dataset.c src/maze_features.c src/maze_sim.c src/maze_generator.c src/maze_graph.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) -Iinclude $(CFLAGS) $(filter %.c,$^) $(LDLIBS) -o $@

build/maze_features_test: tests/maze_features_test.c src/maze_features.c src/maze_graph.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) -Iinclude $(CFLAGS) $(filter %.c,$^) $(LDLIBS) -o $@

build/maze_sim_test: tests/maze_sim_test.c src/maze_sim.c src/maze_graph.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) -Iinclude $(CFLAGS) $(filter %.c,$^) $(LDLIBS) -o $@

build/maze_generator_test: tests/maze_generator_test.c src/maze_generator.c src/maze_graph.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) -Iinclude $(CFLAGS) $(filter %.c,$^) $(LDLIBS) -o $@

build/maze_auth_test: tests/maze_auth_test.c build/maze_auth.o $(HEADERS) | build
	$(CC) $(CPPFLAGS) -Iinclude $(CFLAGS) tests/maze_auth_test.c build/maze_auth.o $(SQLITE_LIBS) -o $@

build/maze_model_score: src/maze_model_score.c model/maze_model.h Makefile | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -Imodel src/maze_model_score.c -o $@

build/maze_graph.o: src/maze_graph.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) -Iinclude $(CFLAGS) -c src/maze_graph.c -o $@

build/maze_generator.o: src/maze_generator.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) -Iinclude $(CFLAGS) -c src/maze_generator.c -o $@

build/maze_auth.o: src/maze_auth.c vendor/sqlite3.h $(HEADERS) | build
	$(CC) $(CPPFLAGS) -Iinclude -Ivendor $(CFLAGS) -c src/maze_auth.c -o $@

build/maze_game: src/maze_game.cpp build/maze_graph.o build/maze_generator.o build/maze_auth.o $(HEADERS) | build
	$(CXX) $(CPPFLAGS) -Iinclude $(RAYLIB_CFLAGS) $(CXXFLAGS) src/maze_game.cpp build/maze_graph.o build/maze_generator.o build/maze_auth.o $(RAYLIB_LIBS) $(SQLITE_LIBS) -o $@

build/maze_cli: src/maze_generator.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) -Iinclude $(CFLAGS) -DMAZE_STANDALONE src/maze_generator.c -o $@

build/menu: src/menu.c build/maze_auth.o $(HEADERS) | build
	$(CC) $(CPPFLAGS) -Iinclude $(RAYLIB_CFLAGS) $(CFLAGS) src/menu.c build/maze_auth.o $(RAYLIB_LIBS) $(SQLITE_LIBS) -o $@

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

check-model: build/maze_model_score
	$(PYTHON) tools/verify_model.py data/maze_data.csv model/maze_model.json ./build/maze_model_score

test: build/maze_features_test build/maze_sim_test build/maze_generator_test build/maze_auth_test build/maze_dataset
	./build/maze_features_test
	./build/maze_sim_test
	./build/maze_generator_test
	mkdir -p build/auth_test && cd build/auth_test && rm -f maze_game.db && ../maze_auth_test
	CC="$(CC)" $(PYTHON) -m unittest discover -s tests -p '*_test.py'
	$(MAKE) check-model
