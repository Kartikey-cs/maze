CC ?= cc
CXX ?= c++
CFLAGS ?= -O2 -Wall -Wextra -Werror -std=c11
CXXFLAGS ?= -O2 -Wall -Wextra -Werror -std=c++17

ifeq ($(OS),Windows_NT)
RAYLIB_LIBS ?= -lraylib -lopengl32 -lgdi32 -lwinmm
else
RAYLIB_LIBS ?= -lraylib
endif

OBJECTS = build/graph.o build/structs.o build/routes.o

all: build/roadmap

build:
	mkdir -p build

build/%.o: %.c %.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/routes.o: routes.cpp routes.hpp graph.h structs.h | build
	$(CXX) $(CXXFLAGS) -c routes.cpp -o $@

build/roadmap: roadmap.cpp routes.hpp graph.h structs.h $(OBJECTS) | build
	$(CXX) $(CXXFLAGS) roadmap.cpp $(OBJECTS) $(RAYLIB_LIBS) -lm -o $@

clean:
	rm -rf build

.PHONY: all clean
