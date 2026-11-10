#ifndef GRAPH_H
#define GRAPH_H

#ifdef __cplusplus
extern "C" {
#endif

/* x and y are meters from the middle of the map. x grows to the east, y grows to the south,
 * so they can be drawn on a screen as they are. */
typedef struct {
    double lat;
    double lon;
    float x;
    float y;
} RoadNode;

typedef struct {
    int to;
    float meters;
} RoadEdge;

/* Directed, weighted graph. The roads leaving node v are edges[first_edge[v]] up to, but not
 * including, edges[first_edge[v + 1]]. */
typedef struct {
    int node_count;
    int edge_count;
    RoadNode *nodes;
    int *first_edge;            /* node_count + 1 entries */
    RoadEdge *edges;
    double centre_lat;          /* the projection is centred here */
    double centre_lon;
    float min_x, min_y, max_x, max_y;
} RoadGraph;

typedef enum {
    ROAD_PLACE_CITY,
    ROAD_PLACE_AREA,
    ROAD_PLACE_LANDMARK,
    ROAD_PLACE_PUMP,
    ROAD_PLACE_HOSPITAL,
    ROAD_PLACE_STREET
} RoadPlaceKind;

typedef struct {
    char name[64];
    int kind;                   /* a RoadPlaceKind */
    int node;                   /* the road node nearest to it */
    float x;
    float y;
} RoadPlace;

/* Reads nodes.csv (lat,lon, one row per node) and edges.csv (from_id,to_id,weight_meters,
 * sorted by from_id, every direction of a road is its own row) from a folder.
 * Returns 1, or prints what is wrong and returns 0. */
int RoadGraph_Load(RoadGraph *graph, const char *dir);
void RoadGraph_Free(RoadGraph *graph);

/* Reads places.csv (name,lat,lon,node,kind). Returns the places, or NULL with *count 0 if the
 * file is missing or wrong. */
RoadPlace *RoadPlace_Load(const RoadGraph *graph, const char *dir, int *count);
const char *RoadPlace_KindName(int kind);

#ifdef __cplusplus
}
#endif

#endif
