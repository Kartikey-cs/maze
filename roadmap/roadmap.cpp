// Standalone road map. Loads the CSV files made by export_osm.py, draws the roads and the
// names of places, and finds routes from a start to an end with the petrol you have.
//
//   ./build/roadmap [data_dir]                                  open the map (default folder: data)
//   ./build/roadmap [data_dir] --from "A" --to "B"              open it with a trip already set
//   ./build/roadmap [data_dir] --from "A" --to "B" --routes     print the routes, no window
//   options for the trip: --petrol L (litres in the tank), --kmpl N (km per litre), --tank L

#include "raylib.h"

#include "routes.hpp"
#include "structs.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <tuple>
#include <vector>

namespace {

const int screenWidth = 1280;
const int screenHeight = 720;
const float maxZoom = 4.0f;                     // pixels per meter
const float rowHeight = 30.0f;
const int maxResults = 8;

const Color backgroundColor = {18, 22, 30, 255};
const Color boxColor = {24, 30, 42, 255};
const Color inkColor = {20, 26, 36, 255};
const Color roadColor = {110, 135, 165, 255};
const Color fromColor = {70, 210, 120, 255};
const Color toColor = {240, 80, 80, 255};

// BFS, DFS, Dijkstra, A*, and the petrol plan: their colours, and their widths in pixels.
// The widest is drawn first, so where two routes share a road you see both colours.
const Color routeColors[5] = {{90, 170, 255, 255}, {170, 230, 60, 255}, {235, 100, 235, 255}, {255, 255, 255, 255}, {40, 225, 205, 255}};
const float routeWidths[5] = {7, 8, 4, 2, 6};
const int drawOrder[5] = {1, 0, 4, 2, 3};

struct LabelStyle {
    float minZoom;                              // pixels per meter, the name shows from here on
    int size;
    Color color;
};

// By kind: city, area, landmark, pump, hospital.
const LabelStyle labelStyles[5] = {{0.0f, 24, {255, 255, 255, 255}}, {0.02f, 17, {245, 225, 160, 255}}, {0.12f, 14, {160, 205, 255, 255}},
                                   {0.35f, 14, {255, 190, 90, 255}}, {0.25f, 14, {255, 150, 150, 255}}};

// What comes first in a search when two names match equally well: cities, then streets last.
const int kindRank[6] = {0, 1, 2, 3, 3, 4};

std::string Lower(std::string text)
{
    for (char& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

std::string Format(double value)
{
    char text[32];
    std::snprintf(text, sizeof text, "%g", value);
    return text;
}

// The text cut to fit a width, with ... at the cut. A text that is being typed keeps its end.
std::string Fit(std::string text, int size, int width, bool keepEnd = false)
{
    while (text.size() > 4 && MeasureText(text.c_str(), size) > width)
    {
        if (keepEnd)
            text.erase(0, 1);
        else
            text = text.substr(0, text.size() - 4) + "...";
    }
    return text;
}

void DrawOutlined(const char* text, int x, int y, int size, Color color)
{
    for (int shift = 0; shift < 4; shift++)
        DrawText(text, x + (shift == 0) - (shift == 1), y + (shift == 2) - (shift == 3), size, Color{10, 14, 20, 230});
    DrawText(text, x, y, size, color);
}

}

class RoadMapViewer {
public:
    RoadMapViewer() { Trie_Init(&trie_); }

    ~RoadMapViewer()
    {
        RoadGraph_Free(&graph_);
        free(places_);
        Trie_Free(&trie_);
    }

    RoadMapViewer(const RoadMapViewer&) = delete;
    RoadMapViewer& operator=(const RoadMapViewer&) = delete;

    bool Load(const std::string& dir)
    {
        if (!RoadGraph_Load(&graph_, dir.c_str()))
            return false;

        places_ = RoadPlace_Load(&graph_, dir.c_str(), &placeCount_);
        for (int id = 0; id < placeCount_; id++)
        {
            Trie_Add(&trie_, places_[id].name, id);
            if (places_[id].kind == ROAD_PLACE_PUMP)
                pumps_.push_back(places_[id]);
        }
        ShowWholeMap();
        fitZoom_ = camera_.zoom;
        return true;
    }

    const Trip& GetTrip() const { return trip_; }

    // The car: how far one litre goes, and how many litres a full tank holds.
    void SetCar(double kmPerLitre, double tankLitres)
    {
        trip_.kmPerLitre = kmPerLitre;
        trip_.tankLitres = tankLitres;
    }

    void SetPetrol(double litres)
    {
        petrol_ = litres;
        text_[2] = Format(litres);
        trip_.Fuel(graph_, pumps_, litres);
    }

    // What matches the typed text, best first: names that start with it, then bigger places
    // (cities before streets), then shorter names.
    std::vector<int> Search(const std::string& text, int max) const
    {
        std::vector<int> found(std::max(placeCount_, 1));
        found.resize(Trie_Find(&trie_, text.c_str(), found.data(), static_cast<int>(found.size())));

        struct Ranked {
            int notPrefix, rank, length, id;
        };
        std::vector<Ranked> ranked;
        std::string typed = Lower(text);

        for (int id : found)
        {
            const RoadPlace& place = places_[id];
            bool prefix = Lower(place.name).compare(0, typed.size(), typed) == 0;
            ranked.push_back({!prefix, kindRank[place.kind], static_cast<int>(std::strlen(place.name)), id});
        }
        std::sort(ranked.begin(), ranked.end(), [](const Ranked& a, const Ranked& b) {
            return std::tie(a.notPrefix, a.rank, a.length, a.id) < std::tie(b.notPrefix, b.rank, b.length, b.id);
        });

        std::vector<int> result;
        for (size_t i = 0; i < ranked.size() && static_cast<int>(i) < max; i++)
            result.push_back(ranked[i].id);
        return result;
    }

    // Sets the start (box 0) or the end (box 1) of the trip. With both set, the routes are
    // worked out. With one, the map goes to that place.
    void Choose(int box, int id)
    {
        const RoadPlace& place = places_[id];

        text_[box] = place.name;
        (box == 0 ? from_ : to_) = id;

        if (from_ >= 0 && to_ >= 0)
        {
            trip_.Run(graph_, pumps_, places_[from_].node, places_[to_].node, petrol_);
            ShowTrip();
            return;
        }
        camera_.target = {place.x, place.y};
        camera_.zoom = place.kind == ROAD_PLACE_CITY ? 0.05f : place.kind == ROAD_PLACE_AREA ? 0.1f : place.kind == ROAD_PLACE_STREET ? 0.28f : 0.6f;
    }

    void Run()
    {
        SetExitKey(KEY_NULL);                       // Esc is for the boxes, the window has its own close button
        while (!WindowShouldClose())
        {
            Update();
            BeginDrawing();
            ClearBackground(backgroundColor);
            Draw();
            EndDrawing();
        }
    }

    void Update()
    {
        Vector2 mouse = GetMousePosition();

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            if (RowAt(mouse) >= 0)
                Pick(RowAt(mouse));
            else
                Focus(BarAt(mouse));
        }

        if (focus_ >= 0)
            Type();
        else
            Shortcuts();

        bool overUi = BarAt(mouse) >= 0 || RowAt(mouse) >= 0 || (trip_.ready && CheckCollisionPointRec(mouse, PanelRect()));
        if (!overUi)
            MoveCamera();
    }

    void Draw()
    {
        BeginMode2D(camera_);
        DrawRoads();
        DrawMarkers();
        DrawRoutes();
        EndMode2D();

        DrawLabels();
        DrawPanel();
        DrawBars();
        DrawRectangle(0, screenHeight - 34, screenWidth, 34, boxColor);
        DrawText("Drag: move   Wheel: zoom   /: search   L: petrol   Tab: next box   1-5: routes   Home: whole map", 14, screenHeight - 28, 18, LIGHTGRAY);
        DrawText("Map data (c) OpenStreetMap contributors", screenWidth - 330, screenHeight - 26, 16, GRAY);
    }

private:
    // The free part of the screen is (left, top) to (right, bottom): the box goes in its middle.
    void ShowBox(float minX, float minY, float maxX, float maxY, float left, float top, float right, float bottom)
    {
        float zoom = std::min((right - left) / std::max(maxX - minX, 50.0f), (bottom - top) / std::max(maxY - minY, 50.0f));

        camera_.zoom = std::min(zoom, maxZoom);
        camera_.offset = {screenWidth / 2.0f, screenHeight / 2.0f};
        camera_.target = {(minX + maxX) / 2 + (screenWidth / 2.0f - (left + right) / 2) / camera_.zoom,
                          (minY + maxY) / 2 + (screenHeight / 2.0f - (top + bottom) / 2) / camera_.zoom};
    }

    void ShowWholeMap()
    {
        ShowBox(graph_.min_x, graph_.min_y, graph_.max_x, graph_.max_y, 40, 40, screenWidth - 40, screenHeight - 40);
    }

    // Zooms so the whole trip is in view, under the boxes and left of the table.
    void ShowTrip()
    {
        float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;

        for (int i : {2, 4})
        {
            for (int v : trip_.route[i].path)
            {
                minX = std::min(minX, graph_.nodes[v].x);
                maxX = std::max(maxX, graph_.nodes[v].x);
                minY = std::min(minY, graph_.nodes[v].y);
                maxY = std::max(maxY, graph_.nodes[v].y);
            }
        }
        if (minX <= maxX)
            ShowBox(minX, minY, maxX, maxY, 70, 110, screenWidth - 460, screenHeight - 70);
    }

    void MoveCamera()
    {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
        {
            camera_.target.x -= GetMouseDelta().x / camera_.zoom;
            camera_.target.y -= GetMouseDelta().y / camera_.zoom;
        }

        float wheel = GetMouseWheelMove();
        if (wheel != 0)
        {
            Vector2 before = GetScreenToWorld2D(GetMousePosition(), camera_);
            camera_.zoom = std::clamp(camera_.zoom * std::pow(1.2f, wheel), fitZoom_ / 2, maxZoom);
            Vector2 after = GetScreenToWorld2D(GetMousePosition(), camera_);
            camera_.target.x += before.x - after.x;
            camera_.target.y += before.y - after.y;
        }
    }

    // The keys when no box is being typed in.
    void Shortcuts()
    {
        bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

        if (IsKeyPressed(KEY_SLASH) || (control && IsKeyPressed(KEY_F)))
            Focus(0);
        else if (IsKeyPressed(KEY_L))
            Focus(2);
        if (IsKeyPressed(KEY_HOME))
            ShowWholeMap();
        for (int i = 0; i < Trip::routeCount; i++)
        {
            if (IsKeyPressed(KEY_ONE + i))
                showRoute_[i] = !showRoute_[i];
        }
    }

    // box 0 is the start, 1 the end, 2 the petrol, -1 is the map.
    void Focus(int box)
    {
        if (box == focus_)
            return;
        if (focus_ == 2)
            text_[2] = Format(petrol_);             // back to the number that is kept
        focus_ = box;
        results_.clear();
        highlight_ = 0;
        if (box == 2)
            text_[2].clear();
        else if (box >= 0)
            results_ = Search(text_[box], maxResults);
        while (GetCharPressed() > 0) {}             // the key that moved the focus is not text
    }

    void Type()
    {
        std::string& text = text_[focus_];
        bool changed = false;
        int count = static_cast<int>(results_.size());

        for (int c = GetCharPressed(); c > 0; c = GetCharPressed())
        {
            bool allowed = focus_ == 2 ? ((c >= '0' && c <= '9') || c == '.') && text.size() < 6 : c >= 32 && c <= 126 && text.size() < 36;
            if (allowed)
            {
                text.push_back(static_cast<char>(c));
                changed = true;
            }
        }
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !text.empty())
        {
            text.pop_back();
            changed = true;
        }
        if (changed)
            TextChanged();

        if (IsKeyPressed(KEY_DOWN) && count > 0)
            highlight_ = (highlight_ + 1) % count;
        if (IsKeyPressed(KEY_UP) && count > 0)
            highlight_ = (highlight_ + count - 1) % count;
        if (RowAt(GetMousePosition()) >= 0)
            highlight_ = RowAt(GetMousePosition());

        if (IsKeyPressed(KEY_ENTER))
        {
            if (focus_ == 2)
                Focus(-1);
            else
                Pick(highlight_);
        }
        else if (IsKeyPressed(KEY_TAB))
            Focus((focus_ + 1) % 3);
        else if (IsKeyPressed(KEY_ESCAPE))
        {
            if (text.empty() || focus_ == 2)
                Focus(-1);
            else
            {
                text.clear();
                TextChanged();
            }
        }
    }

    // The text of the focused box changed: find places, or keep the petrol if it is a number.
    void TextChanged()
    {
        if (focus_ < 2)
        {
            results_ = Search(text_[focus_], maxResults);
            highlight_ = 0;
            return;
        }

        char* end = nullptr;
        double litres = std::strtod(text_[2].c_str(), &end);
        if (end != text_[2].c_str() && *end == '\0' && litres > 0)
        {
            petrol_ = litres;
            trip_.Fuel(graph_, pumps_, litres);
        }
    }

    // Takes a row of the list of results. After the start, the cursor moves on to the end.
    void Pick(int row)
    {
        if (row < 0 || row >= static_cast<int>(results_.size()))
            return;

        int box = focus_;
        Choose(box, results_[row]);
        results_.clear();
        focus_ = -1;
        Focus(box == 0 && to_ < 0 ? 1 : -1);
    }

    int BarAt(Vector2 point) const
    {
        for (int i = 0; i < 3; i++)
        {
            if (CheckCollisionPointRec(point, bars_[i]))
                return i;
        }
        return -1;
    }

    Rectangle ListRect() const
    {
        return {bars_[focus_].x, bars_[focus_].y + 38, bars_[focus_].width, rowHeight * static_cast<float>(results_.size())};
    }

    // The row of the list of results under a point, or -1.
    int RowAt(Vector2 point) const
    {
        if (focus_ < 0 || focus_ > 1 || results_.empty() || !CheckCollisionPointRec(point, ListRect()))
            return -1;
        return static_cast<int>((point.y - ListRect().y) / rowHeight);
    }

    Rectangle PanelRect() const
    {
        return {screenWidth - 434.0f, 58.0f, 420.0f, 62.0f + 26.0f * (trip_.route[4].found ? 5 : 4) + 20.0f * static_cast<float>(trip_.notes.size())};
    }

    Vector2 Point(int node) const { return {graph_.nodes[node].x, graph_.nodes[node].y}; }

    void DrawRoads() const
    {
        Vector2 topLeft = GetScreenToWorld2D({0, 0}, camera_);
        Vector2 bottomRight = GetScreenToWorld2D({screenWidth, screenHeight}, camera_);
        float width = std::max(1.0f, 8.0f * camera_.zoom) / camera_.zoom;      // 8 meters, at least a pixel
        bool joints = width * camera_.zoom >= 3.0f;                            // wide roads need rounded corners

        for (int v = 0; v < graph_.node_count; v++)
        {
            Vector2 a = Point(v);

            for (int i = graph_.first_edge[v]; i < graph_.first_edge[v + 1]; i++)
            {
                Vector2 b = Point(graph_.edges[i].to);
                if (std::max(a.x, b.x) < topLeft.x || std::min(a.x, b.x) > bottomRight.x ||
                    std::max(a.y, b.y) < topLeft.y || std::min(a.y, b.y) > bottomRight.y)
                    continue;
                DrawLineEx(a, b, width, roadColor);
                if (joints)
                    DrawCircleV(b, width / 2, roadColor);
            }
        }
    }

    void DrawMarkers() const
    {
        for (const RoadPlace* place = places_; place < places_ + placeCount_; place++)
        {
            if (place->kind == ROAD_PLACE_PUMP || place->kind == ROAD_PLACE_HOSPITAL)
                DrawCircleV({place->x, place->y}, 6 / camera_.zoom, place->kind == ROAD_PLACE_PUMP ? ORANGE : RED);
        }
    }

    // The routes, the stops of the petrol plan, and the start and the end.
    void DrawRoutes() const
    {
        float pixel = 1.0f / camera_.zoom;

        for (int i : drawOrder)
        {
            const std::vector<int>& path = trip_.route[i].path;
            float width = routeWidths[i] * pixel;

            if (!showRoute_[i])
                continue;
            for (size_t k = 0; k + 1 < path.size(); k++)
            {
                DrawLineEx(Point(path[k]), Point(path[k + 1]), width, routeColors[i]);
                if (width >= 4 * pixel)
                    DrawCircleV(Point(path[k]), width / 2, routeColors[i]);
            }
        }

        if (showRoute_[4])
        {
            for (int node : trip_.stops)
            {
                DrawCircleV(Point(node), 12 * pixel, inkColor);
                DrawCircleV(Point(node), 9 * pixel, routeColors[4]);
            }
        }
        for (int box = 0; box < 2; box++)
        {
            int id = box == 0 ? from_ : to_;
            if (id < 0)
                continue;
            DrawCircleV({places_[id].x, places_[id].y}, 12 * pixel, inkColor);
            DrawCircleV({places_[id].x, places_[id].y}, 9 * pixel, box == 0 ? fromColor : toColor);
            DrawCircleV({places_[id].x, places_[id].y}, 3.5f * pixel, inkColor);
        }
    }

    // The names on the map, the biggest places first. A name that would cover one already
    // drawn is left out, so zoom in to see more. The start and the end are always named.
    void DrawLabels() const
    {
        std::vector<Rectangle> used;

        auto label = [&](int id, const LabelStyle& style, bool pin, Color color) {
            const RoadPlace& place = places_[id];
            Vector2 at = GetWorldToScreen2D({place.x, place.y}, camera_);
            float width = static_cast<float>(MeasureText(place.name, style.size));
            bool centred = place.kind <= ROAD_PLACE_AREA;
            Rectangle box = {centred ? at.x - width / 2 : at.x + 9, at.y - style.size / 2.0f, width, static_cast<float>(style.size)};
            Rectangle padded = {box.x - 3, box.y - 2, box.width + 6, box.height + 4};

            if (!CheckCollisionRecs(box, {0, 0, screenWidth, screenHeight}))
                return;
            if (!pin)
            {
                for (const Rectangle& other : used)
                {
                    if (CheckCollisionRecs(padded, other))
                        return;
                }
                used.push_back(padded);
            }
            else
            {
                DrawRectangleRec(padded, inkColor);
                DrawRectangleLinesEx(padded, 2, color);
            }
            if (place.kind == ROAD_PLACE_LANDMARK && !pin)
                DrawCircleV(at, 3, color);
            DrawOutlined(place.name, static_cast<int>(box.x), static_cast<int>(box.y), style.size, color);
        };

        for (int kind : {ROAD_PLACE_CITY, ROAD_PLACE_AREA, ROAD_PLACE_HOSPITAL, ROAD_PLACE_PUMP, ROAD_PLACE_LANDMARK})
        {
            if (camera_.zoom < labelStyles[kind].minZoom)
                continue;
            for (int id = 0; id < placeCount_; id++)
            {
                if (places_[id].kind == kind && id != from_ && id != to_)
                    label(id, labelStyles[kind], false, labelStyles[kind].color);
            }
        }
        for (int box = 0; box < 2; box++)
        {
            int id = box == 0 ? from_ : to_;
            if (id >= 0)
                label(id, labelStyles[std::min<int>(places_[id].kind, ROAD_PLACE_LANDMARK)], true, box == 0 ? fromColor : toColor);
        }

        if (showRoute_[4])
        {
            for (size_t i = 0; i < trip_.stops.size(); i++)
            {
                Vector2 at = GetWorldToScreen2D(Point(trip_.stops[i]), camera_);
                const char* number = TextFormat("%d", static_cast<int>(i) + 1);
                DrawText(number, static_cast<int>(at.x) - MeasureText(number, 14) / 2, static_cast<int>(at.y) - 7, 14, inkColor);
            }
        }
    }

    void DrawPanel() const
    {
        if (!trip_.ready)
            return;

        Rectangle box = PanelRect();
        int x = static_cast<int>(box.x);
        int y = static_cast<int>(box.y);
        auto right = [&](const std::string& text, int edge, int row, Color color) {
            DrawText(text.c_str(), x + edge - MeasureText(text.c_str(), 16), y + row, 16, color);
        };

        DrawRectangleRec(box, boxColor);
        DrawRectangleLinesEx(box, 1, GRAY);
        DrawText("Routes", x + 12, y + 8, 18, RAYWHITE);
        DrawText("keys 1 to 5 show or hide", x + 90, y + 11, 14, GRAY);
        right("km", 236, 34, GRAY);
        right("litres", 296, 34, GRAY);
        right("checked", 366, 34, GRAY);
        right("ms", 410, 34, GRAY);

        int row = 58;
        for (int i = 0; i < Trip::routeCount; i++)
        {
            const SolveResult& r = trip_.route[i];
            if (i == 4 && !r.found)
                continue;

            Color text = showRoute_[i] ? RAYWHITE : Color{110, 115, 125, 255};
            DrawRectangle(x + 12, y + row + 2, 14, 14, showRoute_[i] ? routeColors[i] : Color{60, 66, 78, 255});
            DrawText(TextFormat("%d %s", i + 1, Trip::Name(i)), x + 32, y + row, 16, text);
            if (!r.found)
            {
                right("no route", 236, row, text);
            }
            else
            {
                right(TextFormat("%.2f", r.meters / 1000.0), 236, row, text);
                right(TextFormat("%.2f", r.meters / 1000.0 / trip_.kmPerLitre), 296, row, text);
                right(i == 4 ? "-" : TextFormat("%d", r.visited), 366, row, text);
                right(TextFormat("%.1f", r.microseconds / 1000.0), 410, row, text);
            }
            row += 26;
        }

        for (size_t i = 0; i < trip_.notes.size(); i++)
        {
            Color color = i == 0 ? LIGHTGRAY : trip_.state == FuelState::Enough ? Color{110, 220, 140, 255}
                        : trip_.state == FuelState::Plan ? Color{255, 200, 80, 255} : Color{255, 110, 110, 255};
            DrawText(Fit(trip_.notes[i], 15, static_cast<int>(box.width) - 24).c_str(), x + 12, y + row + 6 + static_cast<int>(i) * 20, 15, color);
        }
    }

    // The three boxes (start, end, petrol), and the list of results under the one in use.
    void DrawBars() const
    {
        const char* hints[3] = {"From: place or street   ( / )", "To: place or street", ""};

        for (int i = 0; i < 3; i++)
        {
            const Rectangle& bar = bars_[i];
            bool focused = focus_ == i;
            int x = static_cast<int>(bar.x) + 12;
            int y = static_cast<int>(bar.y) + 8;

            DrawRectangleRec(bar, boxColor);
            DrawRectangleLinesEx(bar, 2, focused ? SKYBLUE : GRAY);
            if (i == 2)
            {
                DrawText("Petrol", x - 2, y + 2, 16, GRAY);
                DrawText("L", static_cast<int>(bar.x + bar.width) - 22, y + 2, 16, GRAY);
                x += MeasureText("Petrol", 16) + 6;
            }

            if (text_[i].empty() && !focused)
            {
                DrawText(hints[i], x, y + 1, 18, GRAY);
                continue;
            }
            std::string text = Fit(text_[i], 20, static_cast<int>(bar.x + bar.width) - x - (i == 2 ? 30 : 12), focused);
            DrawText(text.c_str(), x, y, 20, WHITE);
            if (focused && std::fmod(GetTime(), 1.0) < 0.5)
                DrawRectangle(x + 2 + MeasureText(text.c_str(), 20), y, 2, 21, SKYBLUE);
        }

        if (focus_ < 0 || focus_ > 1)
            return;

        Rectangle list = ListRect();
        if (results_.empty())
        {
            if (!text_[focus_].empty())
            {
                DrawRectangleRec({list.x, list.y, list.width, rowHeight}, boxColor);
                DrawText("No places match", static_cast<int>(list.x) + 12, static_cast<int>(list.y) + 7, 18, LIGHTGRAY);
            }
            return;
        }
        DrawRectangleRec(list, boxColor);
        DrawRectangleLinesEx(list, 1, GRAY);
        for (size_t i = 0; i < results_.size(); i++)
        {
            const RoadPlace& place = places_[results_[i]];
            const char* kind = RoadPlace_KindName(place.kind);
            int top = static_cast<int>(list.y + static_cast<float>(i) * rowHeight);

            if (static_cast<int>(i) == highlight_)
                DrawRectangle(static_cast<int>(list.x), top, static_cast<int>(list.width), static_cast<int>(rowHeight), Color{50, 80, 120, 255});
            DrawText(Fit(place.name, 18, static_cast<int>(list.width) - 36 - MeasureText(kind, 15)).c_str(), static_cast<int>(list.x) + 12, top + 6, 18, RAYWHITE);
            DrawText(kind, static_cast<int>(list.x + list.width) - MeasureText(kind, 15) - 12, top + 8, 15, GRAY);
        }
    }

    RoadGraph graph_{};
    RoadPlace* places_ = nullptr;
    int placeCount_ = 0;
    Trie trie_{};
    std::vector<RoadPlace> pumps_;
    Trip trip_;

    Camera2D camera_{};
    float fitZoom_ = 1.0f;
    bool showRoute_[Trip::routeCount] = {true, true, true, true, true};

    // The boxes: start, end and petrol. focus_ is the one being typed in, or -1.
    Rectangle bars_[3] = {{14, 12, 360, 36}, {384, 12, 360, 36}, {754, 12, 150, 36}};
    std::string text_[3];
    int focus_ = -1;
    std::vector<int> results_;
    int highlight_ = 0;

    int from_ = -1;
    int to_ = -1;
    double petrol_ = 5.0;
};

namespace {

// Prints the routes of the trip that is set, like the table on the screen.
void PrintRoutes(const Trip& trip)
{
    if (!trip.ready)
    {
        std::printf("no trip: give both --from and --to\n");
        return;
    }

    std::printf("%-12s %9s %8s %7s %9s %8s\n", "", "km", "litres", "roads", "checked", "ms");
    for (int i = 0; i < Trip::routeCount; i++)
    {
        const SolveResult& r = trip.route[i];
        if (i == 4 && !r.found)
            continue;
        if (!r.found)
        {
            std::printf("%-12s no route\n", Trip::Name(i));
            continue;
        }
        std::printf("%-12s %9.2f %8.2f %7d %9s %8.2f\n", Trip::Name(i), r.meters / 1000.0, r.meters / 1000.0 / trip.kmPerLitre,
                    static_cast<int>(r.path.size()) - 1, i == 4 ? "-" : std::to_string(r.visited).c_str(), r.microseconds / 1000.0);
    }

    for (const std::string& line : trip.notes)
        std::printf("%s\n", line.c_str());
    for (size_t i = 0; i < trip.stops.size(); i++)
        std::printf("  stop %d at road node %d\n", static_cast<int>(i) + 1, trip.stops[i]);
}

}

int main(int argc, char** argv)
{
    std::string dir = "data";
    std::string names[2];                           // the start and the end
    bool routes = false;
    double petrol = 5.0, kmPerLitre = 15.0, tank = 12.0;

    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        bool hasValue = i + 1 < argc;

        if (arg == "--routes")
            routes = true;
        else if (arg == "--from" && hasValue)
            names[0] = argv[++i];
        else if (arg == "--to" && hasValue)
            names[1] = argv[++i];
        else if (arg == "--petrol" && hasValue)
            petrol = std::atof(argv[++i]);
        else if (arg == "--kmpl" && hasValue)
            kmPerLitre = std::atof(argv[++i]);
        else if (arg == "--tank" && hasValue)
            tank = std::atof(argv[++i]);
        else if (arg[0] != '-')
            dir = arg;
        else
        {
            std::fprintf(stderr, "Usage: %s [data_dir] [--from A] [--to B] [--petrol L] [--kmpl N] [--tank L] [--routes]\n", argv[0]);
            return 1;
        }
    }
    if (petrol <= 0 || kmPerLitre <= 0 || tank <= 0)
    {
        std::fprintf(stderr, "--petrol, --kmpl and --tank must be more than 0\n");
        return 1;
    }

    RoadMapViewer viewer;

    if (!viewer.Load(dir))
        return 1;
    viewer.SetCar(kmPerLitre, tank);
    viewer.SetPetrol(petrol);

    for (int box = 0; box < 2; box++)
    {
        if (names[box].empty())
            continue;

        std::vector<int> found = viewer.Search(names[box], 1);
        if (found.empty())
        {
            std::fprintf(stderr, "nothing found for \"%s\"\n", names[box].c_str());
            return 1;
        }
        viewer.Choose(box, found[0]);
    }

    if (routes)
    {
        PrintRoutes(viewer.GetTrip());
        return 0;
    }

    InitWindow(screenWidth, screenHeight, "Road map");
    SetTargetFPS(60);
    viewer.Run();
    CloseWindow();
    return 0;
}
