// maze_solvers.hpp - the solver classes: BFS, DFS, Dijkstra and A*.
// The base class times the search, each child class only says how it searches.
#ifndef MAZE_SOLVERS_HPP
#define MAZE_SOLVERS_HPP

#include "raylib.h"
#include "maze_graph.h"
#include "maze_structs.h"
#include <cstdlib>
#include <vector>

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

#endif
