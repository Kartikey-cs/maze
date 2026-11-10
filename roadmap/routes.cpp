#include "routes.hpp"

#include "structs.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace {

// The length of the shortest road from node a to node b.
float RoadMeters(const RoadGraph& graph, int a, int b)
{
    float best = 1e30f;

    for (int i = graph.first_edge[a]; i < graph.first_edge[a + 1]; i++)
    {
        if (graph.edges[i].to == b)
            best = std::min(best, graph.edges[i].meters);
    }
    return best;
}

std::string Text(const char* format, ...)
{
    char line[200];
    va_list args;

    va_start(args, format);
    std::vsnprintf(line, sizeof line, format, args);
    va_end(args);
    return line;
}

const Solver& SolverAt(int i)
{
    static const BfsSolver bfs;
    static const DfsSolver dfs;
    static const DijkstraSolver dijkstra;
    static const AStarSolver astar;
    const Solver* all[4] = {&bfs, &dfs, &dijkstra, &astar};

    return *all[i];
}

struct FuelPlan {
    bool found = false;
    float meters = 0;
    std::vector<int> stops;     // road nodes of the pumps to stop at
    std::vector<int> path;      // road nodes of the whole route
};

FuelPlan PlanFuel(const RoadGraph& graph, const std::vector<int>& pumpNodes, int start, int goal, double metersNow, double metersFull)
{
    FuelPlan plan;

    if (start == goal)
    {
        plan.found = true;
        plan.path = {start};
        return plan;
    }

    // The points of the trip: the start, each pump once, and the goal.
    std::vector<int> points = {start};
    for (int node : pumpNodes)
    {
        if (node != start && node != goal && std::find(points.begin(), points.end(), node) == points.end())
            points.push_back(node);
    }
    points.push_back(goal);

    int count = static_cast<int>(points.size());
    int last = count - 1;
    DijkstraSolver dijkstra;
    std::vector<Distances> from(last);

    for (int i = 0; i < last; i++)
        from[i] = dijkstra.From(graph, points[i], -1);

    // If the trip starts at a pump, the tank is filled before leaving.
    bool startIsPump = std::find(pumpNodes.begin(), pumpNodes.end(), start) != pumpNodes.end();
    auto range = [&](int i) { return i == 0 && !startIsPump ? metersNow : metersFull; };

    // The shortest chain of legs, each one within the range at its start.
    std::vector<double> total(count, unreached);
    std::vector<int> before(count, -1);
    std::vector<char> settled(count, 0);

    total[0] = 0;
    while (true)
    {
        int i = -1;
        for (int k = 0; k < count; k++)
        {
            if (!settled[k] && total[k] < unreached && (i < 0 || total[k] < total[i]))
                i = k;
        }
        if (i < 0 || i == last)
            break;
        settled[i] = 1;

        for (int j = 1; j < count; j++)
        {
            double leg = from[i].meters[points[j]];
            if (j != i && leg <= range(i) && total[i] + leg < total[j])
            {
                total[j] = total[i] + leg;
                before[j] = i;
            }
        }
    }
    if (total[last] == unreached)
        return plan;

    std::vector<int> chain;                             // the points of the chain, start to goal
    for (int k = last; k != -1; k = before[k])
        chain.push_back(k);
    std::reverse(chain.begin(), chain.end());

    for (size_t k = 0; k + 1 < chain.size(); k++)
    {
        int a = chain[k], b = chain[k + 1];
        std::vector<int> leg;                           // the roads of this leg, from points[a] to points[b]

        for (int v = points[b]; v != -1; v = from[a].cameFrom[v])
        {
            leg.push_back(v);
            if (v == points[a])
                break;
        }
        std::reverse(leg.begin(), leg.end());
        plan.path.insert(plan.path.end(), leg.begin() + (k > 0), leg.end());   // a leg starts where the last one ended
        if (b != last)
            plan.stops.push_back(points[b]);
    }
    plan.found = true;
    plan.meters = static_cast<float>(total[last]);
    return plan;
}

}

SolveResult Solver::Solve(const RoadGraph& graph, int start, int goal) const
{
    SolveResult result;
    if (start < 0 || goal < 0 || start >= graph.node_count || goal >= graph.node_count)
        return result;

    auto before = std::chrono::steady_clock::now();
    Run(graph, start, goal, result);
    result.microseconds = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - before).count();
    return result;
}

void Solver::BuildPath(const RoadGraph& graph, const std::vector<int>& cameFrom, int start, int goal, SolveResult& result)
{
    for (int v = goal; v != -1; v = cameFrom[v])
    {
        result.path.push_back(v);
        if (v == start)
            break;
    }
    std::reverse(result.path.begin(), result.path.end());

    double meters = 0;
    for (size_t i = 1; i < result.path.size(); i++)
        meters += RoadMeters(graph, result.path[i - 1], result.path[i]);
    result.found = true;
    result.meters = static_cast<float>(meters);
}

void BfsSolver::Run(const RoadGraph& graph, int start, int goal, SolveResult& result) const
{
    std::vector<int> cameFrom(graph.node_count, -1);
    std::vector<char> seen(graph.node_count, 0);
    RoadQueue todo;
    int v;

    RoadQueue_Init(&todo, graph.node_count);            // each node is queued once
    seen[start] = 1;
    RoadQueue_Push(&todo, start);

    while (RoadQueue_Pop(&todo, &v))
    {
        result.visited++;
        if (v == goal)
            break;

        for (int i = graph.first_edge[v]; i < graph.first_edge[v + 1]; i++)
        {
            int to = graph.edges[i].to;
            if (!seen[to])
            {
                seen[to] = 1;
                cameFrom[to] = v;
                RoadQueue_Push(&todo, to);
            }
        }
    }
    RoadQueue_Free(&todo);

    if (seen[goal])
        BuildPath(graph, cameFrom, start, goal, result);
}

void DfsSolver::Run(const RoadGraph& graph, int start, int goal, SolveResult& result) const
{
    std::vector<int> cameFrom(graph.node_count, -1);
    std::vector<char> seen(graph.node_count, 0);
    RoadStack todo;
    int v;

    RoadStack_Init(&todo, graph.node_count);            // each node is pushed once
    seen[start] = 1;
    RoadStack_Push(&todo, start);

    while (RoadStack_Pop(&todo, &v))
    {
        result.visited++;
        if (v == goal)
            break;

        for (int i = graph.first_edge[v]; i < graph.first_edge[v + 1]; i++)
        {
            int to = graph.edges[i].to;
            if (!seen[to])
            {
                seen[to] = 1;
                cameFrom[to] = v;
                RoadStack_Push(&todo, to);
            }
        }
    }
    RoadStack_Free(&todo);

    if (seen[goal])
        BuildPath(graph, cameFrom, start, goal, result);
}

Distances DijkstraSolver::From(const RoadGraph& graph, int source, int goal) const
{
    Distances result;
    std::vector<char> done(graph.node_count, 0);
    RoadHeap heap;
    int v;

    result.meters.assign(graph.node_count, unreached);
    result.cameFrom.assign(graph.node_count, -1);
    RoadHeap_Init(&heap, graph.edge_count + 1);         // a node is pushed at most once per road

    result.meters[source] = 0;
    RoadHeap_Push(&heap, 0, source);

    while (RoadHeap_Pop(&heap, &v))
    {
        if (done[v])
            continue;                                   // an older, longer entry for v
        done[v] = 1;
        result.visited++;
        if (v == goal)
            break;

        for (int i = graph.first_edge[v]; i < graph.first_edge[v + 1]; i++)
        {
            int to = graph.edges[i].to;
            double longer = result.meters[v] + graph.edges[i].meters;
            if (longer < result.meters[to])
            {
                result.meters[to] = longer;
                result.cameFrom[to] = v;
                RoadHeap_Push(&heap, longer + (goal >= 0 ? Guess(graph, to, goal) : 0), to);
            }
        }
    }
    RoadHeap_Free(&heap);
    return result;
}

void DijkstraSolver::Run(const RoadGraph& graph, int start, int goal, SolveResult& result) const
{
    Distances found = From(graph, start, goal);

    result.visited = found.visited;
    if (found.meters[goal] != unreached)
        BuildPath(graph, found.cameFrom, start, goal, result);
}

// The straight line to the goal on the flat map. The flat map stretches distances by a
// fraction of a percent, so the guess is cut a little to stay below the real road length.
double AStarSolver::Guess(const RoadGraph& graph, int v, int goal) const
{
    return 0.99 * std::hypot(graph.nodes[v].x - graph.nodes[goal].x, graph.nodes[v].y - graph.nodes[goal].y);
}

const char* Trip::Name(int i)
{
    return i < 4 ? SolverAt(i).Name() : "Petrol plan";
}

void Trip::Run(const RoadGraph& graph, const std::vector<RoadPlace>& pumps, int from, int to, double litres)
{
    start = from;
    goal = to;
    for (int i = 0; i < 4; i++)
        route[i] = SolverAt(i).Solve(graph, start, goal);
    ready = true;
    Fuel(graph, pumps, litres);
}

void Trip::Fuel(const RoadGraph& graph, const std::vector<RoadPlace>& pumps, double litres)
{
    route[4] = SolveResult{};
    stops.clear();
    notes.clear();
    state = FuelState::None;
    if (!ready || !route[2].found)
        return;

    double now = litres * kmPerLitre * 1000.0;                              // how far the petrol takes you
    double full = std::max(tankLitres, litres) * kmPerLitre * 1000.0;       // how far a full tank does
    double shortest = route[2].meters;
    double missing = (shortest - now) / 1000.0 / kmPerLitre;

    notes.push_back(Text("%g L is %.1f km. The shortest trip, %.1f km, needs %.1f L.", litres, now / 1000.0, shortest / 1000.0,
                         shortest / 1000.0 / kmPerLitre));
    if (shortest <= now)
    {
        state = FuelState::Enough;
        notes.push_back("Enough petrol, no stop needed.");
        return;
    }

    std::vector<int> pumpNodes;
    for (const RoadPlace& pump : pumps)
        pumpNodes.push_back(pump.node);

    auto before = std::chrono::steady_clock::now();
    FuelPlan plan = PlanFuel(graph, pumpNodes, start, goal, now, full);
    double microseconds = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - before).count();

    if (!plan.found)
    {
        state = FuelState::Impossible;
        notes.push_back(Text("%.1f L short, and no chain of pumps gets you there.", missing));
        notes.push_back(Text("A full tank (%.0f L) is %.1f km.", std::max(tankLitres, litres), full / 1000.0));
        return;
    }

    state = FuelState::Plan;
    route[4].found = true;
    route[4].meters = plan.meters;
    route[4].microseconds = microseconds;
    route[4].path = plan.path;
    stops = plan.stops;

    double planLitres = plan.meters / 1000.0 / kmPerLitre;
    if (stops.empty())
    {
        // Only possible when the trip starts at a pump: the tank is filled before leaving.
        notes.push_back(Text("You start at a pump: fill up first. Trip %.1f km, %.1f L.", plan.meters / 1000.0, planLitres));
        return;
    }
    notes.push_back(Text("%.1f L short. Plan: %d stop%s, %.1f km, %.1f L in all.", missing, static_cast<int>(stops.size()),
                         stops.size() == 1 ? "" : "s", plan.meters / 1000.0, planLitres));

    std::string line = "Stops: ";
    for (size_t i = 0; i < stops.size(); i++)
    {
        std::string name = "pump";
        for (const RoadPlace& pump : pumps)
        {
            if (pump.node == stops[i])
            {
                name = pump.name;
                break;
            }
        }
        line += (i ? ", " : "") + std::to_string(i + 1) + " " + name;
    }
    notes.push_back(line);
}
