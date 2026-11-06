// Frontline 2022 — simulation core (no rendering, no SDL).
// Original code. Map: 80x64 grid, ~17 km per cell. One tick = 6 hours.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace fl {

constexpr int W = 80, H = 64, N = W * H;
constexpr double LON0 = 22.0, LON_STEP = 0.25, LAT0 = 53.5, LAT_STEP = 0.15;

enum Side : int { UA = 0, RU = 1 };
enum Terr : uint8_t { LAND = 0, SEA = 1, BLOCK = 2 };

struct City {
    std::string name;  // English name; UI may localise
    double lon, lat;
    int x = 0, y = 0;
    int value = 1;     // strategic value 1..5
    int home = UA;     // side whose rear it is
    bool rear = false; // never falls (far-west supply source) or foreign base
    bool base = false; // permanent supply source for `home`
};

struct Cell {
    Terr t = BLOCK;
    int8_t owner = -1;     // -1: nobody (sea / foreign)
    bool inUA = false;     // inside Ukraine's internationally recognised borders
    bool river = false;
    float rough = 1.f;     // terrain defence multiplier (forest / hills)
    float cap = 0.f;       // capture progress
    int city = -1;
};

struct Brigade {
    int id = 0, side = UA, num = 1;
    int x = 0, y = 0, tx = -1, ty = -1;
    float men = 2000, maxMen = 2400, org = 100, sup = 1, exp = 0, dug = 0, mp = 0;
    bool fighting = false, alive = true, ai = true;
    int orderDay = 0;
};

struct Stats {
    int cells[2] = {0, 0};      // cells inside Ukraine held by each side
    int total = 0;              // total cells inside Ukraine
    int brigades[2] = {0, 0};
    long manpower[2] = {0, 0};
    long lost[2] = {0, 0};
};

enum Winner { NONE_YET = -1, WIN_UA = 0, WIN_RU = 1, DRAW = 2 };

class World {
public:
    Cell cells[N];
    std::vector<City> cities;
    std::vector<Brigade> brig;
    float sup[2][N];
    int day = 0, tick = 0;
    int kyivDays = 0;          // consecutive days Kyiv is held by RU
    int winner = NONE_YET;
    long lost[2] = {0, 0};
    int nextId = 1, counter[2] = {0, 0};
    uint32_t rng = 12345;

    int player = -1;           // side controlled by the human (-1: observer)
    bool assist = false;       // AI also commands the player's idle brigades
    void init(uint32_t seed = 1, int playerSide = -1);
    void step();               // advance one 6-hour tick
    Stats stats() const;
    void order(int brigadeId, int x, int y);

    static int idx(int x, int y) { return y * W + x; }
    static bool in(int x, int y) { return x >= 0 && y >= 0 && x < W && y < H; }
    Cell &at(int x, int y) { return cells[idx(x, y)]; }
    const Cell &at(int x, int y) const { return cells[idx(x, y)]; }
    static void lonlatToCell(double lon, double lat, int &x, int &y);
    Brigade *find(int id);

private:
    uint32_t rnd();
    float frand() { return (rnd() & 0xFFFFFF) / float(0x1000000); }
    void buildMap();
    void spawnStart();
    Brigade &spawn(int side, int x, int y, float men, float org, float exp);
    bool placeNear(int side, double lon, double lat, int &x, int &y) const;
    void computeSupply();
    void moveAndFight();
    void retreatCheck();
    void updateOwnership();
    void daily();
    void reinforce();
    void aiDay(int side);
    bool nextStep(const Brigade &b, int &nx, int &ny) const;
    void combat(Brigade &a, int cx, int cy);
    float localMen(int side, int x, int y, int r) const;
    bool enemyAt(int side, int x, int y) const;
};

} // namespace fl
