#ifndef MAZE_SOLVERS_HPP
#define MAZE_SOLVERS_HPP

#include "maze_graph.h"

#include <vector>

struct SolveResult {
    bool found = false;
    int pathLength = 0;         // moves from the start to the goal
    int visited = 0;            // squares the solver actually looked at
    double microseconds = 0;    // how long one run took
    std::vector<int> path;      // vertex ids from the start to the goal
};

// Every solver finds a route through a MazeGraph. They share this parent class
// and each child only says how it searches.
class Solver {
public:
    virtual ~Solver() = default;

    virtual const char* Name() const = 0;

    // Runs the search and times it.
    SolveResult Solve(const MazeGraph& graph, int start, int goal) const;

protected:
    virtual void Run(const MazeGraph& graph, int start, int goal, SolveResult& result) const = 0;

    // Follows the "came from" list back from the goal to fill in the path.
    static void BuildPath(const std::vector<int>& cameFrom, int start, int goal, SolveResult& result);
};

// Breadth first search with a queue. Always finds a shortest route.
class BfsSolver : public Solver {
public:
    const char* Name() const override { return "BFS"; }
protected:
    void Run(const MazeGraph& graph, int start, int goal, SolveResult& result) const override;
};

// Depth first search with a stack. Finds a route, not always the shortest.
class DfsSolver : public Solver {
public:
    const char* Name() const override { return "DFS"; }
protected:
    void Run(const MazeGraph& graph, int start, int goal, SolveResult& result) const override;
};

// Dijkstra with a heap. Every move costs 1 here, so it finds the same routes as
// BFS, but it is the version that also works when moves cost different amounts.
class DijkstraSolver : public Solver {
public:
    const char* Name() const override { return "Dijkstra"; }
protected:
    void Run(const MazeGraph& graph, int start, int goal, SolveResult& result) const override;
};

// A*: Dijkstra plus a guess of the distance left (straight line along the
// grid), so it usually looks at far fewer squares.
class AStarSolver : public Solver {
public:
    const char* Name() const override { return "A*"; }
protected:
    void Run(const MazeGraph& graph, int start, int goal, SolveResult& result) const override;
};

#endif
