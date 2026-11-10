#include "graph.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EARTH_RADIUS 6371008.8      /* meters */
#define PI 3.14159265358979323846
#define LINE_MAX_LENGTH 512

static const char *const kind_names[] = {"city", "area", "landmark", "pump", "hospital", "street"};

const char *RoadPlace_KindName(int kind)
{
    return kind >= 0 && kind < 6 ? kind_names[kind] : "place";
}

static int IsBlank(const char *line)
{
    return line[0] == '\n' || line[0] == '\r' || line[0] == '\0';
}

/* Opens a CSV from the folder, counts its rows and leaves the file at the first row. */
static FILE *OpenCsv(const char *dir, const char *name, int *rows)
{
    char path[512], line[LINE_MAX_LENGTH];

    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *file = fopen(path, "r");
    if (!file)
    {
        fprintf(stderr, "cannot open %s\n", path);
        return NULL;
    }

    *rows = 0;
    fgets(line, sizeof line, file);                 /* the header */
    while (fgets(line, sizeof line, file))
        *rows += !IsBlank(line);
    rewind(file);
    fgets(line, sizeof line, file);
    return file;
}

static void Project(const RoadGraph *graph, double lat, double lon, float *x, float *y)
{
    *x = (float)(EARTH_RADIUS * (lon - graph->centre_lon) * PI / 180.0 * cos(graph->centre_lat * PI / 180.0));
    *y = (float)(-EARTH_RADIUS * (lat - graph->centre_lat) * PI / 180.0);
}

void RoadGraph_Free(RoadGraph *graph)
{
    free(graph->nodes);
    free(graph->first_edge);
    free(graph->edges);
    memset(graph, 0, sizeof *graph);
}

static int ReadNodes(RoadGraph *graph, const char *dir)
{
    char line[LINE_MAX_LENGTH];
    int rows;
    FILE *file = OpenCsv(dir, "nodes.csv", &rows);

    if (!file)
        return 0;
    if (rows == 0)
    {
        fprintf(stderr, "nodes.csv has no nodes\n");
        fclose(file);
        return 0;
    }

    graph->nodes = (RoadNode *)malloc((size_t)rows * sizeof(RoadNode));
    if (!graph->nodes)
    {
        fprintf(stderr, "out of memory\n");
        fclose(file);
        return 0;
    }

    double min_lat = 90, max_lat = -90, min_lon = 180, max_lon = -180;

    while (fgets(line, sizeof line, file))
    {
        RoadNode *node = &graph->nodes[graph->node_count];

        if (IsBlank(line))
            continue;
        if (sscanf(line, "%lf,%lf", &node->lat, &node->lon) != 2 || node->lat < -90 || node->lat > 90 ||
            node->lon < -180 || node->lon > 180)
        {
            fprintf(stderr, "nodes.csv: bad row %d (expected lat,lon)\n", graph->node_count + 1);
            fclose(file);
            return 0;
        }
        graph->node_count++;
        if (node->lat < min_lat) min_lat = node->lat;
        if (node->lat > max_lat) max_lat = node->lat;
        if (node->lon < min_lon) min_lon = node->lon;
        if (node->lon > max_lon) max_lon = node->lon;
    }
    fclose(file);

    graph->centre_lat = (min_lat + max_lat) / 2;
    graph->centre_lon = (min_lon + max_lon) / 2;
    for (int v = 0; v < graph->node_count; v++)
    {
        RoadNode *node = &graph->nodes[v];

        Project(graph, node->lat, node->lon, &node->x, &node->y);
        if (v == 0 || node->x < graph->min_x) graph->min_x = node->x;
        if (v == 0 || node->x > graph->max_x) graph->max_x = node->x;
        if (v == 0 || node->y < graph->min_y) graph->min_y = node->y;
        if (v == 0 || node->y > graph->max_y) graph->max_y = node->y;
    }
    return 1;
}

static int ReadEdges(RoadGraph *graph, const char *dir)
{
    char line[LINE_MAX_LENGTH];
    int rows;
    FILE *file = OpenCsv(dir, "edges.csv", &rows);

    if (!file)
        return 0;

    graph->edges = (RoadEdge *)malloc((size_t)(rows > 0 ? rows : 1) * sizeof(RoadEdge));
    graph->first_edge = (int *)malloc((size_t)(graph->node_count + 1) * sizeof(int));
    if (!graph->edges || !graph->first_edge)
    {
        fprintf(stderr, "out of memory\n");
        fclose(file);
        return 0;
    }

    int next_node = 0;          /* the first node whose first_edge is not set yet */
    int previous_from = 0;

    while (fgets(line, sizeof line, file))
    {
        int from, to;
        float meters;

        if (IsBlank(line))
            continue;
        if (sscanf(line, "%d,%d,%f", &from, &to, &meters) != 3 || from < 0 || from >= graph->node_count ||
            to < 0 || to >= graph->node_count || meters < 0 || from < previous_from)
        {
            fprintf(stderr, "edges.csv: bad row %d (expected from_id,to_id,meters, sorted by from_id)\n", graph->edge_count + 1);
            fclose(file);
            return 0;
        }
        previous_from = from;
        while (next_node <= from)
            graph->first_edge[next_node++] = graph->edge_count;

        graph->edges[graph->edge_count].to = to;
        graph->edges[graph->edge_count].meters = meters;
        graph->edge_count++;
    }
    fclose(file);

    while (next_node <= graph->node_count)
        graph->first_edge[next_node++] = graph->edge_count;
    return 1;
}

int RoadGraph_Load(RoadGraph *graph, const char *dir)
{
    memset(graph, 0, sizeof *graph);
    if (ReadNodes(graph, dir) && ReadEdges(graph, dir))
        return 1;
    RoadGraph_Free(graph);
    return 0;
}

RoadPlace *RoadPlace_Load(const RoadGraph *graph, const char *dir, int *count)
{
    char line[LINE_MAX_LENGTH];
    int rows;
    FILE *file = OpenCsv(dir, "places.csv", &rows);

    *count = 0;
    if (!file)
        return NULL;

    RoadPlace *places = (RoadPlace *)malloc((size_t)(rows > 0 ? rows : 1) * sizeof(RoadPlace));

    while (places && fgets(line, sizeof line, file))
    {
        RoadPlace *place = &places[*count];
        double lat, lon;
        char kind[16];

        if (IsBlank(line))
            continue;
        place->kind = -1;
        if (sscanf(line, "%63[^,],%lf,%lf,%d,%15s", place->name, &lat, &lon, &place->node, kind) == 5)
        {
            for (int k = 0; k < 6; k++)
            {
                if (strcmp(kind, kind_names[k]) == 0)
                    place->kind = k;
            }
        }
        if (place->kind < 0 || place->node < 0 || place->node >= graph->node_count)
        {
            fprintf(stderr, "places.csv: bad row %d (expected name,lat,lon,node,kind)\n", *count + 1);
            free(places);
            places = NULL;
            *count = 0;
            break;
        }
        Project(graph, lat, lon, &place->x, &place->y);
        (*count)++;
    }
    fclose(file);
    return places;
}
