#ifndef ROUTES_HPP
#define ROUTES_HPP

#include "graph.h"

#include <string>
#include <vector>

struct SolveResult {
    bool found = false;
    float meters = 0;           // length of the route
    int visited = 0;            // road nodes the solver looked at
    double microseconds = 0;
    std::vector<int> path;      // road nodes from the start to the goal
};

// Base class for the solvers. Each child only says how it searches.
class Solver {
public:
    virtual ~Solver() = default;

    virtual const char* Name() const = 0;

    // Runs the search and times it. Roads are followed in their direction, so one-way
    // streets are respected.
    SolveResult Solve(const RoadGraph& graph, int start, int goal) const;

protected:
    virtual void Run(const RoadGraph& graph, int start, int goal, SolveResult& result) const = 0;

    // Follows the "came from" list back from the goal to fill in the path and its length.
    static void BuildPath(const RoadGraph& graph, const std::vector<int>& cameFrom, int start, int goal, SolveResult& result);
};

// BFS with a queue. Finds the route with the fewest roads, not the shortest one.
class BfsSolver : public Solver {
public:
    const char* Name() const override { return "BFS"; }
protected:
    void Run(const RoadGraph& graph, int start, int goal, SolveResult& result) const override;
};

// DFS with a stack. Finds a route, usually a long winding one.
class DfsSolver : public Solver {
public:
    const char* Name() const override { return "DFS"; }
protected:
    void Run(const RoadGraph& graph, int start, int goal, SolveResult& result) const override;
};

// The shortest road distance from one node, and the way there.
struct Distances {
    std::vector<double> meters;
    std::vector<int> cameFrom;
    int visited = 0;
};

constexpr double unreached = 1e300;

// Dijkstra with a heap. Finds the shortest route in meters.
class DijkstraSolver : public Solver {
public:
    const char* Name() const override { return "Dijkstra"; }

    // The distance from source to every node, or only until goal is reached (goal -1: all).
    Distances From(const RoadGraph& graph, int source, int goal) const;

protected:
    void Run(const RoadGraph& graph, int start, int goal, SolveResult& result) const override;

    // Guess of the meters left to the goal. Dijkstra has none.
    virtual double Guess(const RoadGraph&, int, int) const { return 0; }
};

// A* is Dijkstra plus a guess of the distance left, so it usually looks at fewer nodes.
class AStarSolver : public DijkstraSolver {
public:
    const char* Name() const override { return "A*"; }
protected:
    double Guess(const RoadGraph& graph, int v, int goal) const override;
};

// The petrol: does the trip fit in the tank, and if not, a trip with stops at petrol pumps.
// The car fills up completely at any pump and burns petrol in proportion to the distance.
// The plan is the shortest trip in which no stretch between two stops is longer than the
// petrol allows. How: find the shortest road distance from the start and from every pump to
// every pump and to the goal, keep the legs that one tank can drive, and take the shortest
// chain of legs.
enum class FuelState { None, Enough, Plan, Impossible };

// Everything about one trip: the four routes, the petrol plan and the lines that tell about it.
struct Trip {
    static constexpr int routeCount = 5;

    bool ready = false;
    int start = -1;
    int goal = -1;
    double kmPerLitre = 15.0;
    double tankLitres = 12.0;

    SolveResult route[routeCount];      // BFS, DFS, Dijkstra, A*, and the petrol plan
    std::vector<int> stops;             // road nodes of the pumps the plan stops at, in order
    FuelState state = FuelState::None;
    std::vector<std::string> notes;     // lines about the petrol

    static const char* Name(int i);

    // Runs the four solvers, then Fuel.
    void Run(const RoadGraph& graph, const std::vector<RoadPlace>& pumps, int from, int to, double litres);

    // Works out if the petrol is enough, and if not, plans a trip with stops at the pumps.
    void Fuel(const RoadGraph& graph, const std::vector<RoadPlace>& pumps, double litres);
};

#endif
