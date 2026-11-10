"""Gets the drivable roads, petrol pumps, hospitals, places and street names of a city from
OpenStreetMap and writes nodes.csv, edges.csv and places.csv for the C++ program. Run it once;
the program itself does not need Python.

    python export_osm.py                                  a 20 km square round the Clock Tower of Dehradun, into data/
    python export_osm.py --around "30.3245,78.0413" --radius 10000 --out data_city

Needs OSMnx 2.x:  pip install osmnx
Map data (c) OpenStreetMap contributors, licensed under the ODbL.
"""
import argparse
import csv
import math
import os
import sys
import time
import unicodedata

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
M_PER_DEGREE = 111320.0
CITY_PLACES = {"city", "town"}
AREA_PLACES = {"suburb", "neighbourhood", "quarter", "locality", "village", "hamlet"}

# Places for the labels and the search.
PLACE_TAGS = {
    "place": ["city", "town", "village", "suburb", "neighbourhood", "quarter", "locality", "hamlet"],
    "amenity": ["university", "college", "school", "marketplace", "bus_station", "cinema", "theatre",
                "townhall", "courthouse", "police", "library", "place_of_worship"],
    "tourism": ["attraction", "museum", "hotel", "zoo", "viewpoint", "gallery"],
    "leisure": ["park", "stadium"],
    "railway": ["station"],
    "public_transport": ["station"],
    "shop": ["mall"],
    "man_made": ["tower"],
    "historic": True,
}

# What to ask OpenStreetMap for, and which kind each find is. A pump or hospital without a
# name is kept as "unnamed", any other place without a name is dropped.
SOURCES = [
    ("petrol pumps", {"amenity": "fuel"}, lambda row: "pump", True),
    ("hospitals", {"amenity": "hospital"}, lambda row: "hospital", True),
    ("places", PLACE_TAGS, lambda row: ("city" if row["place"] in CITY_PLACES else "area" if row["place"] in AREA_PLACES else None)
     if isinstance(row.get("place"), str) else "landmark", False),
]


def clean_name(*candidates):
    """The first usable name as plain ASCII without commas (the C loader splits on commas).
    Accents are dropped from the letter (Rajpur written with a long a stays Rajpur)."""
    for value in candidates:
        if isinstance(value, str):
            text = unicodedata.normalize("NFKD", value).encode("ascii", "ignore").decode()
            text = " ".join(text.replace(",", " ").replace('"', " ").split())
            if text:
                return text[:63]
    return "unnamed"


def first_text(value):
    """OSMnx gives a text, or a list of texts (in no fixed order) when pieces of road were
    merged. The first in alphabetical order is taken, so every export gives the same files."""
    texts = sorted(v for v in (value if isinstance(value, (list, tuple)) else [value]) if isinstance(v, str) and v.strip())
    return texts[0] if texts else None


def nearest_node(lats, lons, lat, lon):
    """Index of the road node closest to a point."""
    dy = (lats - lat) * M_PER_DEGREE
    dx = (lons - lon) * M_PER_DEGREE * math.cos(math.radians(lat))
    return int(np.argmin(dx * dx + dy * dy))


def get_features(ox, centre, radius, tags):
    """The OpenStreetMap features with these tags, or None if there are none. The public server
    often answers "busy": the request is tried again with longer and longer waits."""
    wait = 30
    for attempt in range(1, 6):
        try:
            return ox.features.features_from_point(centre, tags=tags, dist=radius)
        except Exception as problem:
            if type(problem).__name__ == "InsufficientResponseError":
                return None                                 # nothing found, which is an answer
            if attempt == 5:
                raise
            print(f"  failed ({str(problem).strip()[:90]}); trying again in {wait} s ({attempt}/5)", flush=True)
            time.sleep(wait)
            wait *= 2


def drop_copies(places):
    """Drops copies of the same place (a point and a building): the same name within 300 m, or
    within 30 m when either has no name. A place is (name, lat, lon, kind)."""
    kept = []
    for name, lat, lon, kind in places:
        for other, olat, olon, _ in kept:
            distance = math.hypot((olat - lat) * M_PER_DEGREE, (olon - lon) * M_PER_DEGREE * math.cos(math.radians(lat)))
            same = other.lower() == name.lower()
            if (distance < 300 and same and name != "unnamed") or (distance < 30 and (same or "unnamed" in (name, other))):
                break
        else:
            kept.append((name, lat, lon, kind))
    return kept


def export(args):
    import osmnx as ox
    if int(ox.__version__.split(".")[0]) < 2:
        sys.exit("This script needs OSMnx 2.x. Upgrade with: pip install -U osmnx")

    centre = tuple(float(part) for part in args.around.split(","))
    try:
        print("Getting the road network...")
        graph = ox.graph.graph_from_point(centre, dist=args.radius, network_type="drive")
        # One-way streets can leave parts that cannot be left. Keep the part where every node can
        # reach every other node, so any start and end have a route.
        graph = ox.truncate.largest_component(graph, strongly=True)

        found = []
        for label, tags, kind_of, keep_unnamed in SOURCES:
            print(f"Getting the {label}...", flush=True)
            features = get_features(ox, centre, args.radius, tags)
            places = []
            for _, row in (features.iterrows() if features is not None else []):
                kind = kind_of(row)
                name = clean_name(row.get("name:en"), row.get("name"))
                if kind and (keep_unnamed or name != "unnamed"):
                    point = row.geometry.representative_point()         # a building counts as its middle
                    places.append((name, point.y, point.x, kind))
            found.append(drop_copies(places))
    except Exception as problem:
        sys.exit(f"\nCould not get the data: {str(problem).strip()[:200]}\n"
                 "Run the same command again: what was downloaded is kept in cache/, so it is quicker.")

    os.makedirs(args.out, exist_ok=True)
    node_ids = list(graph.nodes)
    index = {osmid: i for i, osmid in enumerate(node_ids)}          # OSM ids are huge, the program wants 0, 1, 2, ...
    lats = np.array([graph.nodes[n]["y"] for n in node_ids])
    lons = np.array([graph.nodes[n]["x"] for n in node_ids])

    # Two roads can join the same pair of nodes. The routing only needs the shorter one (with
    # the same length, the name that comes first, so every export gives the same files).
    best = {}
    for u, v, data in graph.edges(data=True):
        key = (index[u], index[v])
        road = (max(float(data["length"]), 0.1), clean_name(first_text(data.get("name")), first_text(data.get("ref"))))
        if u != v and (key not in best or road < best[key]):
            best[key] = road

    # A street is one place on the map: the node of the street nearest to the middle of all its nodes.
    members = {}
    for (a, b), (_, name) in best.items():
        if name != "unnamed":
            members.setdefault(name, set()).update((a, b))
    streets = []
    for name, nodes in sorted(members.items()):
        nodes = np.array(sorted(nodes))
        node = int(nodes[nearest_node(lats[nodes], lons[nodes], lats[nodes].mean(), lons[nodes].mean())])
        streets.append((name, lats[node], lons[node], "street"))

    def write(name, header, rows):
        with open(os.path.join(args.out, name), "w", newline="", encoding="utf-8") as f:
            out = csv.writer(f, lineterminator="\n")
            out.writerow(header)
            out.writerows(rows)

    write("nodes.csv", ["lat", "lon"], ([f"{lat:.7f}", f"{lon:.7f}"] for lat, lon in zip(lats, lons)))
    write("edges.csv", ["from_id", "to_id", "weight_meters"], ([a, b, f"{best[a, b][0]:.1f}"] for a, b in sorted(best)))
    places = [p for kept in found for p in kept] + streets
    write("places.csv", ["name", "lat", "lon", "node", "kind"],
          ([name, f"{lat:.7f}", f"{lon:.7f}", nearest_node(lats, lons, lat, lon), kind] for name, lat, lon, kind in places))

    print(f"\n{len(lats)} nodes, {len(best)} directed roads, {len(places)} places ({len(streets)} of them streets). Written to {args.out}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--around", default="30.3245,78.0413", help='the middle of the map, "lat,lon"')
    parser.add_argument("--radius", type=int, default=10000, help="half the side of the square, in meters (default 10000)")
    parser.add_argument("--out", default=os.path.join(HERE, "data"))
    export(parser.parse_args())


if __name__ == "__main__":
    main()
