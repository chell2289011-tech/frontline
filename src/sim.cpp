#include "sim.hpp"

#include <algorithm>
#include <cmath>
#include <queue>

namespace fl {

// ---------------------------------------------------------------- helpers
struct P { double lon, lat; };

static bool pip(const std::vector<P> &poly, double x, double y) {
    bool in = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        if (((poly[i].lat > y) != (poly[j].lat > y)) &&
            (x < (poly[j].lon - poly[i].lon) * (y - poly[i].lat) / (poly[j].lat - poly[i].lat) + poly[i].lon))
            in = !in;
    }
    return in;
}

static float hash2(int x, int y, uint32_t s) {
    uint32_t h = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u + s * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return ((h ^ (h >> 16)) & 0xFFFF) / 65535.f;
}
static float vnoise(float x, float y, uint32_t s) {
    int xi = int(std::floor(x)), yi = int(std::floor(y));
    float fx = x - xi, fy = y - yi;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    float a = hash2(xi, yi, s), b = hash2(xi + 1, yi, s), c = hash2(xi, yi + 1, s), d = hash2(xi + 1, yi + 1, s);
    return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy;
}

static float dist(int ax, int ay, int bx, int by) {
    float dx = std::abs(ax - bx), dy = std::abs(ay - by);
    return std::max(dx, dy) + 0.4f * std::min(dx, dy);
}

// Approximate outline of Ukraine (lon, lat), mainland.
static const std::vector<P> UA_POLY = {
    {23.6, 51.6},  {24.2, 51.75}, {25.8, 51.93}, {27.3, 51.6},  {28.7, 51.4},  {30.5, 51.35}, {30.8, 51.8},
    {31.8, 52.1},  {32.5, 52.3},  {33.5, 52.35}, {34.4, 51.75}, {35.3, 51.05}, {35.9, 50.45}, {36.6, 50.25},
    {38.0, 49.9},  {39.2, 49.7},  {40.1, 49.6},  {39.7, 48.8},  {39.8, 47.95}, {38.7, 47.8},  {38.2, 47.1},
    {37.3, 47.0},  {36.3, 46.8},  {35.3, 46.4},  {34.0, 46.2},  {33.0, 46.1},  {32.5, 46.4},  {31.6, 46.55},
    {31.0, 46.6},  {30.7, 46.35}, {30.2, 45.85}, {29.7, 45.25}, {28.25, 45.45}, {28.5, 46.4},  {29.2, 46.55},
    {30.1, 46.45}, {30.0, 47.4},  {29.5, 47.95}, {29.2, 48.4},  {28.3, 48.1},  {26.7, 48.3},  {26.2, 48.25},
    {24.9, 47.7},  {24.0, 47.95}, {22.9, 47.95}, {22.1, 48.4},  {22.55, 49.05}, {23.0, 49.1}, {23.5, 50.4}};
static const std::vector<P> CRIMEA_POLY = {{33.7, 46.2}, {35.0, 46.0}, {36.6, 45.4}, {36.0, 45.0}, {35.4, 45.0},
                                            {34.4, 44.5}, {33.6, 44.4}, {32.6, 45.4}, {33.3, 45.95}};
static const std::vector<P> DNIPRO = {{30.6, 51.5}, {30.5, 50.45}, {31.4, 50.0}, {32.05, 49.44}, {33.42, 49.07},
                                      {34.2, 48.7}, {35.05, 48.46}, {35.14, 47.84}, {34.4, 47.4}, {33.37, 46.75},
                                      {32.62, 46.64}};

struct CityDef { const char *name; double lon, lat; int value, home; bool rear, base; };
static const CityDef CITY_DEFS[] = {
    {"Kyiv", 30.52, 50.45, 5, UA, false, false},       {"Kharkiv", 36.23, 49.99, 4, UA, false, false},
    {"Lviv", 24.03, 49.84, 3, UA, true, false},        {"Odesa", 30.72, 46.48, 4, UA, false, false},
    {"Dnipro", 35.05, 48.46, 4, UA, false, false},     {"Zaporizhzhia", 35.14, 47.84, 3, UA, false, false},
    {"Mariupol", 37.55, 47.10, 3, UA, false, false},   {"Donetsk", 37.80, 48.00, 3, UA, false, false},
    {"Luhansk", 39.30, 48.57, 2, UA, false, false},    {"Kherson", 32.62, 46.64, 3, UA, false, false},
    {"Mykolaiv", 32.00, 46.97, 3, UA, false, false},   {"Sevastopol", 33.52, 44.62, 3, UA, false, false},
    {"Simferopol", 34.10, 44.95, 2, UA, false, false}, {"Chernihiv", 31.29, 51.50, 2, UA, false, false},
    {"Sumy", 34.80, 50.90, 2, UA, false, false},       {"Poltava", 34.55, 49.59, 2, UA, false, false},
    {"Vinnytsia", 28.47, 49.23, 2, UA, true, false},   {"Zhytomyr", 28.66, 50.25, 2, UA, false, false},
    {"Lutsk", 25.34, 50.75, 1, UA, true, false},       {"Chernivtsi", 25.93, 48.29, 1, UA, true, false},
    {"Uzhhorod", 22.30, 48.62, 1, UA, true, false},    {"Kremenchuk", 33.42, 49.07, 1, UA, false, false},
    {"Kropyvnytskyi", 32.26, 48.50, 2, UA, false, false}, {"Melitopol", 35.37, 46.84, 2, UA, false, false},
    {"Sievierodonetsk", 38.50, 48.95, 2, UA, false, false}, {"Bakhmut", 38.00, 48.60, 1, UA, false, false},
    // Russian / Belarusian rear bases
    {"Belgorod", 36.58, 50.60, 1, RU, true, true},     {"Kursk", 36.19, 51.73, 1, RU, true, true},
    {"Bryansk", 34.36, 53.24, 1, RU, true, true},      {"Gomel", 31.00, 52.43, 1, RU, true, true},
    {"Mozyr", 29.20, 52.05, 1, RU, true, true},        {"Voronezh", 39.20, 51.67, 1, RU, true, true},
    {"Rostov", 39.70, 47.23, 1, RU, true, true},       {"Krasnodar", 38.97, 45.04, 1, RU, true, true},
};

// ---------------------------------------------------------------- basics
uint32_t World::rnd() {
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return rng;
}

void World::lonlatToCell(double lon, double lat, int &x, int &y) {
    x = int((lon - LON0) / LON_STEP);
    y = int((LAT0 - lat) / LAT_STEP);
    x = std::max(0, std::min(W - 1, x));
    y = std::max(0, std::min(H - 1, y));
}

Brigade *World::find(int id) {
    for (auto &b : brig) if (b.id == id && b.alive) return &b;
    return nullptr;
}

void World::order(int id, int x, int y) {
    Brigade *b = find(id);
    if (!b || !in(x, y) || at(x, y).t == BLOCK) return;
    b->tx = x; b->ty = y; b->orderDay = day; b->ai = false;
}

void World::buildMap() {
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            Cell &c = at(x, y);
            c = Cell();
            double lon = LON0 + (x + 0.5) * LON_STEP, lat = LAT0 - (y + 0.5) * LAT_STEP;
            bool crimea = pip(CRIMEA_POLY, lon, lat);
            bool ua = crimea || pip(UA_POLY, lon, lat);
            c.inUA = ua;
            if (ua) {
                c.t = LAND; c.owner = UA;
                if (crimea) c.owner = RU;
                if ((lon >= 38.0 && lat >= 47.3) || (lon >= 37.65 && lat >= 47.7 && lat <= 48.7)) c.owner = RU;
            } else if (lon >= 33.5 && lat >= 46.9) { c.t = LAND; c.owner = RU; }
            else if (lon >= 38.4 && (lat >= 46.9 || lat < 46.0)) { c.t = LAND; c.owner = RU; }
            else if (lat >= 51.3 && lon >= 23.9 && lon < 33.5) { c.t = LAND; c.owner = RU; } // Belarus (RU staging)
            else if (lat <= 46.9 && lon >= 29.0) c.t = SEA;
            else c.t = BLOCK;
            if (c.t == LAND) {
                float n = vnoise(x / 4.5f, y / 4.5f, 7) * 0.7f + vnoise(x / 2.0f, y / 2.0f, 3) * 0.3f;
                c.rough = 1.f + 0.55f * std::max(0.f, n - 0.5f);
                if (lon < 25.6 && lat < 49.3 && ua) c.rough += 0.25f; // Carpathians
            }
        }
    for (size_t i = 0; i + 1 < DNIPRO.size(); i++)
        for (int s = 0; s <= 40; s++) {
            double t = s / 40.0, lon = DNIPRO[i].lon + (DNIPRO[i + 1].lon - DNIPRO[i].lon) * t,
                   lat = DNIPRO[i].lat + (DNIPRO[i + 1].lat - DNIPRO[i].lat) * t;
            int x, y; lonlatToCell(lon, lat, x, y);
            if (at(x, y).t == LAND && at(x, y).inUA) at(x, y).river = true;
        }
    cities.clear();
    for (const auto &d : CITY_DEFS) {
        City c; c.name = d.name; c.lon = d.lon; c.lat = d.lat; c.value = d.value; c.home = d.home;
        c.rear = d.rear; c.base = d.base;
        lonlatToCell(d.lon, d.lat, c.x, c.y);
        if (at(c.x, c.y).t != LAND) { // snap to nearest land cell
            bool done = false;
            for (int r = 1; r < 4 && !done; r++)
                for (int dy = -r; dy <= r && !done; dy++)
                    for (int dx = -r; dx <= r && !done; dx++)
                        if (in(c.x + dx, c.y + dy) && at(c.x + dx, c.y + dy).t == LAND) { c.x += dx; c.y += dy; done = true; }
        }
        at(c.x, c.y).city = int(cities.size());
        cities.push_back(c);
    }
}

bool World::placeNear(int side, double lon, double lat, int &x, int &y) const {
    int cx, cy; lonlatToCell(lon, lat, cx, cy);
    for (int r = 0; r < 8; r++)
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++) {
                if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                int nx = cx + dx, ny = cy + dy;
                if (!in(nx, ny)) continue;
                const Cell &c = at(nx, ny);
                if (c.t == LAND && c.owner == side) { x = nx; y = ny; return true; }
            }
    return false;
}

Brigade &World::spawn(int side, int x, int y, float men, float org, float exp) {
    Brigade b;
    b.id = nextId++; b.side = side; b.num = ++counter[side];
    b.x = x; b.y = y; b.men = men; b.maxMen = std::max(men, 2200.f); b.org = org; b.exp = exp;
    b.sup = sup[side][idx(x, y)];
    b.ai = (side != player) || assist;
    brig.push_back(b);
    return brig.back();
}

void World::spawnStart() {
    struct S { int side; double lon, lat; int n; float men, org, exp; };
    static const S S0[] = {
        {RU, 30.0, 52.0, 3, 2600, 95, 0.1}, {RU, 31.0, 52.2, 2, 2500, 95, 0.1}, {RU, 29.2, 51.9, 2, 2400, 90, 0.1},
        {RU, 32.8, 52.4, 2, 2400, 92, 0.1}, {RU, 35.0, 51.6, 2, 2400, 92, 0.1}, {RU, 36.3, 50.7, 3, 2600, 95, 0.1},
        {RU, 38.6, 49.3, 2, 2300, 90, 0.2}, {RU, 38.2, 48.3, 3, 2300, 90, 0.3}, {RU, 39.0, 47.6, 2, 2300, 90, 0.2},
        {RU, 38.0, 47.5, 1, 2300, 90, 0.2}, {RU, 33.8, 45.9, 3, 2500, 95, 0.1}, {RU, 35.0, 45.8, 2, 2500, 95, 0.1},
        {RU, 39.7, 47.2, 3, 2400, 85, 0.0}, {RU, 36.6, 50.6, 2, 2400, 85, 0.0},
        {UA, 30.5, 50.5, 3, 2280, 90, 0.2}, {UA, 30.1, 51.0, 2, 2160, 90, 0.2}, {UA, 31.3, 51.5, 2, 2040, 88, 0.2},
        {UA, 34.8, 50.9, 2, 2040, 88, 0.2}, {UA, 36.2, 50.0, 3, 2280, 90, 0.2}, {UA, 34.5, 49.6, 1, 2040, 85, 0.1},
        {UA, 37.0, 48.8, 3, 2400, 92, 0.5}, {UA, 37.3, 48.0, 3, 2400, 92, 0.5}, {UA, 37.4, 47.2, 2, 2280, 90, 0.4},
        {UA, 35.1, 47.8, 2, 2160, 88, 0.2}, {UA, 35.0, 48.5, 1, 2040, 85, 0.1}, {UA, 32.0, 47.0, 2, 2160, 88, 0.2},
        {UA, 32.6, 46.7, 1, 2040, 88, 0.2}, {UA, 30.7, 46.5, 2, 2040, 85, 0.1}, {UA, 32.3, 48.5, 1, 2040, 85, 0.1},
        {UA, 24.0, 49.8, 2, 2040, 80, 0.0}, {UA, 28.7, 50.3, 1, 2040, 85, 0.1},
    };
    for (const auto &s : S0)
        for (int i = 0; i < s.n; i++) {
            int x, y;
            if (placeNear(s.side, s.lon, s.lat, x, y)) spawn(s.side, x, y, s.men * (0.9f + 0.2f * frand()), s.org, s.exp);
        }
}

void World::init(uint32_t seed, int playerSide) {
    player = playerSide; assist = false;
    rng = seed * 2654435761u + 1; if (!rng) rng = 1;
    brig.clear(); day = tick = 0; kyivDays = 0; winner = NONE_YET; nextId = 1;
    counter[0] = counter[1] = 0; lost[0] = lost[1] = 0;
    buildMap();
    for (auto &s : sup) for (auto &v : s) v = 0.f;
    computeSupply();
    spawnStart();
    computeSupply();
}

// ---------------------------------------------------------------- supply
void World::computeSupply() {
    static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1}, DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    static std::vector<char> blocked;
    blocked.assign(2 * N, 0); // cells containing an enemy brigade of side s's enemy
    for (const auto &b : brig) if (b.alive) blocked[(1 - b.side) * N + idx(b.x, b.y)] = 1;
    for (int s = 0; s < 2; s++) {
        std::vector<float> d(N, 1e9f);
        typedef std::pair<float, int> Q;
        std::priority_queue<Q, std::vector<Q>, std::greater<Q>> pq;
        for (const auto &c : cities) {
            const Cell &cell = at(c.x, c.y);
            if (cell.owner == s && ((c.base && c.home == s) || c.value >= 2)) { d[idx(c.x, c.y)] = 0; pq.push({0.f, idx(c.x, c.y)}); }
        }
        while (!pq.empty()) {
            auto [dd, i] = pq.top(); pq.pop();
            if (dd > d[i]) continue;
            int x = i % W, y = i / W;
            for (int k = 0; k < 8; k++) {
                int nx = x + DX[k], ny = y + DY[k];
                if (!in(nx, ny)) continue;
                const Cell &c = at(nx, ny);
                if (c.t == BLOCK) continue;
                int ni = idx(nx, ny);
                float cost = (k < 4 ? 1.f : 1.41f) * (c.owner == s ? 1.f : 1.8f) + (blocked[s * N + ni] ? 4.f : 0.f);
                if (c.t == SEA) cost = 3.f;
                if (dd + cost < d[ni]) { d[ni] = dd + cost; pq.push({d[ni], ni}); }
            }
        }
        for (int i = 0; i < N; i++) sup[s][i] = std::max(0.f, std::min(1.f, 1.f - d[i] / 32.f));
    }
}

// ---------------------------------------------------------------- movement & combat
bool World::enemyAt(int side, int x, int y) const {
    for (const auto &b : brig) if (b.alive && b.side != side && b.x == x && b.y == y) return true;
    return false;
}

float World::localMen(int side, int x, int y, int r) const {
    float m = 0;
    for (const auto &b : brig)
        if (b.alive && b.side == side && std::abs(b.x - x) <= r && std::abs(b.y - y) <= r) m += b.men;
    return m;
}

bool World::nextStep(const Brigade &b, int &nx, int &ny) const {
    static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1}, DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    static int prev[N];
    static int q[N];
    std::fill(prev, prev + N, -2);
    int start = idx(b.x, b.y), goal = idx(b.tx, b.ty);
    if (start == goal) return false;
    int qh = 0, qt = 0; q[qt++] = start; prev[start] = -1;
    while (qh < qt && prev[goal] == -2) {
        int i = q[qh++], x = i % W, y = i / W;
        for (int k = 0; k < 8; k++) {
            int ax = x + DX[k], ay = y + DY[k];
            if (!in(ax, ay) || cells[idx(ax, ay)].t == BLOCK) continue;
            if (k >= 4 && (cells[idx(ax, y)].t == BLOCK || cells[idx(x, ay)].t == BLOCK)) continue;
            int ni = idx(ax, ay);
            if (prev[ni] != -2) continue;
            prev[ni] = i; q[qt++] = ni;
        }
    }
    if (prev[goal] == -2) return false;
    int cur = goal;
    while (prev[cur] != start) cur = prev[cur];
    nx = cur % W; ny = cur / W;
    return true;
}

static float power(const Brigade &b) {
    return b.men * (0.3f + 0.7f * b.org / 100.f) * (0.4f + 0.6f * b.sup) * (1.f + 0.3f * b.exp);
}

void World::combat(Brigade &a, int cx, int cy) {
    const Cell &c = at(cx, cy);
    float terr = c.rough * (c.river ? 1.3f : 1.f) * (c.city >= 0 ? 1.f + 0.25f * cities[c.city].value : 1.f);
    float dp = 0, defMen = 0;
    std::vector<Brigade *> defs;
    for (auto &d : brig)
        if (d.alive && d.side != a.side && d.x == cx && d.y == cy) {
            defs.push_back(&d); dp += power(d) * (1.f + 0.8f * d.dug) * terr; defMen += d.men;
        }
    if (defs.empty()) return;
    float r = std::max(0.25f, std::min(4.f, power(a) / std::max(dp, 1.f)));
    float lossA = a.men * 0.010f / r;
    float lossD = defMen * 0.010f * r;
    a.men -= lossA; lost[a.side] += long(lossA);
    a.org -= 1.8f / r; a.fighting = true; a.dug = 0;
    a.exp = std::min(2.f, a.exp + 0.002f);
    for (auto *d : defs) {
        float share = d->men / defMen, l = lossD * share;
        d->men -= l; lost[d->side] += long(l);
        d->org -= 1.5f * r * 0.8f; d->dug = std::max(0.f, d->dug - 0.012f * r);
        d->fighting = true; d->exp = std::min(2.f, d->exp + 0.002f);
    }
}

void World::moveAndFight() {
    std::vector<int> order(brig.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = int(i);
    for (size_t i = order.size(); i > 1; i--) std::swap(order[i - 1], order[rnd() % i]);
    for (auto &b : brig) b.fighting = false;
    for (int oi : order) {
        Brigade &b = brig[oi];
        if (!b.alive) continue;
        bool moved = false;
        if (enemyAt(b.side, b.x, b.y)) combat(b, b.x, b.y);
        else if (b.tx >= 0) {
            if (b.x == b.tx && b.y == b.ty) b.tx = b.ty = -1;
            else {
                int nx, ny;
                if (!nextStep(b, nx, ny)) b.tx = b.ty = -1;
                else if (enemyAt(b.side, nx, ny)) combat(b, nx, ny);
                else {
                    const Cell &c = at(nx, ny);
                    b.mp += 0.28f * (0.5f + 0.5f * b.sup) / ((c.river ? 2.f : 1.f) * (1.f + 0.5f * (c.rough - 1.f)));
                    if (b.mp >= 1.f) { b.mp = 0; b.x = nx; b.y = ny; moved = true; b.dug = 0; }
                }
            }
        }
        // upkeep
        float target = sup[b.side][idx(b.x, b.y)];
        b.sup += (target - b.sup) * 0.25f;
        if (!b.fighting) {
            b.org = std::min(100.f, b.org + (b.sup > 0.4f ? 1.5f : -0.5f));
            if (b.sup > 0.5f && b.men < b.maxMen) b.men = std::min(b.maxMen, b.men + b.maxMen * 0.0025f);
            if (!moved) b.dug = std::min(1.f, b.dug + 0.012f);
        }
        if (b.sup < 0.15f) { b.org -= 1.f; float l = b.men * 0.003f; b.men -= l; lost[b.side] += long(l); }
    }
}

void World::retreatCheck() {
    static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1}, DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    for (auto &b : brig) {
        if (!b.alive) continue;
        if (b.men < 150.f) { b.alive = false; continue; }
        if (b.org < 12.f || b.men < 0.25f * b.maxMen) {
            int bx = -1, by = -1; float best = -1;
            for (int k = 0; k < 8; k++) {
                int nx = b.x + DX[k], ny = b.y + DY[k];
                if (!in(nx, ny) || at(nx, ny).t != LAND || at(nx, ny).owner != b.side || enemyAt(b.side, nx, ny)) continue;
                float s = sup[b.side][idx(nx, ny)];
                if (s > best) { best = s; bx = nx; by = ny; }
            }
            if (bx < 0) { b.alive = false; lost[b.side] += long(b.men); continue; }
            b.x = bx; b.y = by; b.org = 30.f; b.tx = b.ty = -1; b.mp = 0; b.dug = 0;
        }
    }
}

void World::updateOwnership() {
    static float inf[2][N];
    static char has[2][N];
    for (int s = 0; s < 2; s++) { std::fill(inf[s], inf[s] + N, 0.f); std::fill(has[s], has[s] + N, 0); }
    for (const auto &b : brig) {
        if (!b.alive) continue;
        has[b.side][idx(b.x, b.y)] = 1;
        float v = b.men / 1000.f * (0.3f + 0.7f * b.org / 100.f);
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                int nx = b.x + dx, ny = b.y + dy;
                if (in(nx, ny)) inf[b.side][idx(nx, ny)] += (dx == 0 && dy == 0) ? v : v * 0.35f;
            }
    }
    for (int i = 0; i < N; i++) {
        Cell &c = cells[i];
        if (c.t != LAND || c.owner < 0) continue;
        int o = c.owner, e = 1 - o;
        if (!has[o][i] && inf[e][i] > 0.25f && inf[e][i] > 1.5f * inf[o][i]) {
            c.cap += std::min(0.5f, inf[e][i] * 0.2f);
            if (c.cap >= 1.f) { c.owner = int8_t(e); c.cap = 0; }
        } else c.cap = std::max(0.f, c.cap - 0.05f);
    }
}

// ---------------------------------------------------------------- daily logic
void World::reinforce() {
    int cnt[2] = {0, 0};
    for (const auto &b : brig) if (b.alive) cnt[b.side]++;
    if (day % 3 == 0 && cnt[UA] < 70) {
        std::vector<int> cand;
        for (size_t i = 0; i < cities.size(); i++) {
            const City &c = cities[i];
            if (c.home == UA && c.value >= 2 && at(c.x, c.y).owner == UA) {
                cand.push_back(int(i)); if (c.rear) cand.push_back(int(i));
            }
        }
        if (!cand.empty()) {
            const City &c = cities[cand[rnd() % cand.size()]];
            spawn(UA, c.x, c.y, 1900 + frand() * 400, 75, 0.15f);
        }
    }
    if (day % 3 == 0 && cnt[RU] < 90) {
        std::vector<int> cand;
        for (size_t i = 0; i < cities.size(); i++)
            if (cities[i].home == RU && cities[i].base && at(cities[i].x, cities[i].y).owner == RU) cand.push_back(int(i));
        if (!cand.empty()) {
            const City &c = cities[cand[rnd() % cand.size()]];
            spawn(RU, c.x, c.y, 2400 + frand() * 400, 80, 0.05f);
        }
    }
}

static float objective(const City &c, int side) { // how attractive a city is for an attacker `side`
    return float(c.value);
}

void World::aiDay(int side) {
    int enemy = 1 - side;
    // frontier lists
    std::vector<int> enemyFront, myFront;
    static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1}, DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const Cell &c = at(x, y);
            if (c.t != LAND || c.owner < 0) continue;
            bool adj = false;
            for (int k = 0; k < 8 && !adj; k++) {
                int nx = x + DX[k], ny = y + DY[k];
                if (in(nx, ny) && at(nx, ny).t == LAND && at(nx, ny).owner == 1 - c.owner) adj = true;
            }
            if (!adj) continue;
            if (c.owner == enemy) { if (c.inUA) enemyFront.push_back(idx(x, y)); }
            else myFront.push_back(idx(x, y));
        }
    std::vector<int> tcount(N, 0);
    for (const auto &b : brig)
        if (b.alive && b.side == side && b.tx >= 0) tcount[idx(b.tx, b.ty)]++;

    // garrisons: every valuable city we hold gets a defender
    for (const auto &c : cities) {
        if (c.rear || c.value < 3 || at(c.x, c.y).owner != side) continue;
        bool covered = false;
        for (const auto &b : brig)
            if (b.alive && b.side == side && ((b.x == c.x && b.y == c.y) || (b.tx == c.x && b.ty == c.y))) { covered = true; break; }
        if (covered) continue;
        Brigade *best = nullptr; float bd = 1e9f;
        for (auto &b : brig) {
            if (!b.alive || b.side != side || !b.ai || b.men < 0.5f * b.maxMen) continue;
            float d = dist(b.x, b.y, c.x, c.y);
            if (d < bd) { bd = d; best = &b; }
        }
        if (best) { best->tx = c.x; best->ty = c.y; best->orderDay = day + 6; }
    }
    for (auto &b : brig) {
        if (!b.alive || b.side != side || !b.ai) continue;
        bool weak = b.men < 0.45f * b.maxMen || b.org < 35.f;
        if (weak) { // pull back to rest
            int bx = -1, by = -1; float best = 1e9f;
            for (int i : myFront) {
                int x = i % W, y = i / W;
                // rest area: own cell far from the enemy influence
                (void)x; (void)y;
            }
            for (int y = std::max(0, b.y - 8); y < std::min(H, b.y + 9); y++)
                for (int x = std::max(0, b.x - 8); x < std::min(W, b.x + 9); x++) {
                    const Cell &c = at(x, y);
                    if (c.t != LAND || c.owner != side || sup[side][idx(x, y)] < 0.6f) continue;
                    if (localMen(enemy, x, y, 3) > 0) continue;
                    float d = dist(b.x, b.y, x, y);
                    if (d < best) { best = d; bx = x; by = y; }
                }
            if (bx >= 0) { b.tx = bx; b.ty = by; b.orderDay = day; }
            continue;
        }
        bool needNew = b.tx < 0 || day - b.orderDay > 10;
        if (!needNew) continue;
        int bestCell = -1; float bestScore = -1e9f;
        if (side == RU) {
            for (int i : enemyFront) {
                int x = i % W, y = i / W;
                if (tcount[i] >= 2) continue;
                float obj = 0;
                for (const auto &c : cities) if (c.home == UA && !c.rear) obj += objective(c, side) / (1.f + dist(x, y, c.x, c.y) / 5.f);
                if (at(x, y).city >= 0) obj += 1.5f * cities[at(x, y).city].value;
                float score = obj - 0.12f * dist(b.x, b.y, x, y) - 0.0009f * localMen(enemy, x, y, 1) * (b.men < 1500 ? 2.f : 1.f);
                if (score > bestScore) { bestScore = score; bestCell = i; }
            }
        } else {
            // counter-attack when locally superior, otherwise reinforce the most threatened frontier cell
            float mine = localMen(side, b.x, b.y, 3), theirs = localMen(enemy, b.x, b.y, 3);
            if (theirs > 0 && mine > 1.7f * theirs && b.org > 70.f && b.men > 0.7f * b.maxMen && frand() < 0.35f) {
                float bs = 1e9f;
                for (int i : enemyFront) {
                    int x = i % W, y = i / W;
                    float d = dist(b.x, b.y, x, y);
                    if (d > 7 || tcount[i] >= 2) continue;
                    float s = d + localMen(enemy, x, y, 1) / 800.f - (at(x, y).city >= 0 ? 3.f : 0.f);
                    if (s < bs) { bs = s; bestCell = i; }
                }
            }
            if (bestCell < 0)
                for (int i : myFront) {
                    int x = i % W, y = i / W;
                    if (tcount[i] >= 3) continue;
                    float threat = localMen(enemy, x, y, 3) - 0.7f * localMen(side, x, y, 2);
                    float cv = 0;
                    for (const auto &c : cities) if (c.home == UA && at(c.x, c.y).owner == UA) cv += c.value / (1.f + dist(x, y, c.x, c.y) / 4.f);
                    float score = threat * 0.0007f + cv * 0.35f - 0.09f * dist(b.x, b.y, x, y);
                    if (score > bestScore) { bestScore = score; bestCell = i; }
                }
        }
        if (bestCell >= 0) { b.tx = bestCell % W; b.ty = bestCell / W; b.orderDay = day; tcount[bestCell]++; }
    }
}

Stats World::stats() const {
    Stats s;
    for (int i = 0; i < N; i++) if (cells[i].inUA) { s.total++; if (cells[i].owner >= 0) s.cells[cells[i].owner]++; }
    for (const auto &b : brig) if (b.alive) { s.brigades[b.side]++; s.manpower[b.side] += long(b.men); }
    s.lost[0] = lost[0]; s.lost[1] = lost[1];
    return s;
}

void World::daily() {
    computeSupply();
    reinforce();
    aiDay(UA);
    aiDay(RU);
    // victory conditions
    for (const auto &c : cities)
        if (c.name == "Kyiv") kyivDays = (at(c.x, c.y).owner == RU) ? kyivDays + 1 : 0;
    Stats s = stats();
    if (kyivDays >= 30 || s.cells[RU] > 0.6 * s.total) winner = WIN_RU;
    else if (s.cells[RU] == 0) winner = WIN_UA;
    else if (day >= 730) winner = DRAW;
    day++;
}

void World::step() {
    if (winner != NONE_YET) return;
    tick++;
    moveAndFight();
    retreatCheck();
    updateOwnership();
    brig.erase(std::remove_if(brig.begin(), brig.end(), [](const Brigade &b) { return !b.alive; }), brig.end());
    if (tick % 4 == 0) daily();
}

} // namespace fl
