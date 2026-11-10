# Road map (standalone)

A road map of a city from OpenStreetMap, with a search bar, four route finders and a petrol
planner. It is separate from the maze game: nothing here is linked to `src/` or `include/`.

Pick a start, an end and the litres of petrol you have. The program finds the route with BFS,
DFS, Dijkstra and A*, says if the petrol is enough, and if not, plans a trip with stops at
petrol pumps.

## Try it

`sample/` is a small made-up city, not a real place.

```sh
make                      # on Windows: mingw32-make CC=gcc CXX=g++
./build/roadmap sample
```

| Key | What it does |
| --- | --- |
| drag, wheel | move and zoom the map |
| `/`, or click a box | type in **From** (a place or a street). Up and down choose, `Enter` or a click picks |
| **To** box | the end of the trip. After the start is picked, the cursor moves here |
| `L`, or click **Petrol** | the litres in the tank. Type a number |
| `Tab`, `Esc` | next box, clear the text and leave the box |
| `1` to `5` | show or hide a route (BFS, DFS, Dijkstra, A*, petrol plan) |
| `Home` | the whole map |

Without a window: `./build/roadmap data_city --from "graphic era" --to "clock tower" --petrol 0.4 --routes`
prints the table of routes. `--kmpl` (default 15) and `--tank` (default 12 L) set the car.

## Real data: Dehradun

```sh
pip install osmnx
python export_osm.py --out data_city        # a 20 km square round the Clock Tower, a few minutes
./build/roadmap data_city
```

`--around "lat,lon"` and `--radius` pick another place. The public Overpass server is
sometimes busy: the script tries again with longer waits, and never writes half a result.
The files are the same on every run. Only the export needs Python.

What OpenStreetMap does not have is a limit of the data, not of the program: only about 7%
of the roads in Dehradun have a name, and some pumps have none (shown as `unnamed`). Names are
plain ASCII: accents are removed and Hindi script is dropped.

## Files

| File | What it does |
| --- | --- |
| `export_osm.py` | Gets the roads, places and street names, writes the CSV files |
| `structs.c/.h` | C: our own queue, stack, min-heap and trie |
| `graph.c/.h` | C: the graph in compressed rows, the CSV loaders, the map projection |
| `routes.cpp/.hpp` | C++: the `Solver` classes and the petrol plan (`Trip`) |
| `roadmap.cpp` | C++ with Raylib: the window, the boxes, the names on the map, the table |

CSV files, in one folder:

| File | Columns |
| --- | --- |
| `nodes.csv` | `lat,lon`, one row per road node. The row number is the node id |
| `edges.csv` | `from_id,to_id,weight_meters`, sorted by `from_id`. Every direction of a road is its own row |
| `places.csv` | `name,lat,lon,node,kind`. `node` is the nearest road node. `kind` is `city`, `area`, `landmark`, `pump`, `hospital` or `street` |

`places.csv` is optional.

## How it works

- **Graph:** directed and weighted, in compressed rows: `edges` holds every road grouped by
  where it starts, and `first_edge[v]` says where the roads of node `v` begin. Only the largest
  strongly connected part of the network is kept, so any start and end have a route.
- **Projection:** equirectangular around the middle of the map, in meters. At city size the
  error is small.
- **Solvers:** BFS uses a queue and finds the route with the fewest roads. DFS uses a stack
  and finds a long winding route. Dijkstra uses a min-heap and finds the shortest. A* is
  Dijkstra plus a guess of the distance left (0.99 times the straight line, so it never
  guesses too high). On 200 random trips in Dehradun, Dijkstra and BFS agreed with networkx
  every time, and A* checked about 4 times fewer nodes than Dijkstra.
- **Petrol:** the shortest trip needs `km / (km per litre)` litres. If the tank is short, the
  planner finds the shortest road distance from the start and from every pump to every pump
  and to the end, keeps the legs one tank can drive, and takes the shortest chain of them.
  Assumed: petrol use is in proportion to the distance, the tank is filled completely at a
  pump, stops are free, and the car does 15 km per litre. Only pumps that OpenStreetMap has are
  used (26 in the city).
- **Search:** a trie over every word of every name. Every word you type has to be the start of
  some word in the name, in any order: `raj` and `road` both find "Rajpur Road". Names that
  start with the text come first, then cities before streets.

## Not done

Clicking the map to pick a start and an end (that needs a spatial index for the nearest road
node), curved roads (a road is a straight line between two junctions), traffic, hills, the
price of petrol, toll and closed roads.

Map data (c) OpenStreetMap contributors, licensed under the ODbL.
