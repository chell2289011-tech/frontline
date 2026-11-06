// Frontline 2022 — SDL2 front-end. Original code.
#include <SDL.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

#include "sim.hpp"

using namespace fl;

static const int CELL = 12, MAPW = W * CELL, MAPH = H * CELL, PANEL = 320, WIN_W = MAPW + PANEL, WIN_H = MAPH;

static SDL_Renderer *R;
static TTF_Font *F12, *F14, *F20, *F32;

static const std::map<std::string, const char *> UKR = {
    {"Kyiv", "Київ"}, {"Kharkiv", "Харків"}, {"Lviv", "Львів"}, {"Odesa", "Одеса"}, {"Dnipro", "Дніпро"},
    {"Zaporizhzhia", "Запоріжжя"}, {"Mariupol", "Маріуполь"}, {"Donetsk", "Донецьк"}, {"Luhansk", "Луганськ"},
    {"Kherson", "Херсон"}, {"Mykolaiv", "Миколаїв"}, {"Sevastopol", "Севастополь"}, {"Simferopol", "Сімферополь"},
    {"Chernihiv", "Чернігів"}, {"Sumy", "Суми"}, {"Poltava", "Полтава"}, {"Vinnytsia", "Вінниця"},
    {"Zhytomyr", "Житомир"}, {"Lutsk", "Луцьк"}, {"Chernivtsi", "Чернівці"}, {"Uzhhorod", "Ужгород"},
    {"Kremenchuk", "Кременчук"}, {"Kropyvnytskyi", "Кропивницький"}, {"Melitopol", "Мелітополь"},
    {"Sievierodonetsk", "Сєвєродонецьк"}, {"Bakhmut", "Бахмут"}, {"Belgorod", "Бєлгород"}, {"Kursk", "Курськ"},
    {"Bryansk", "Брянськ"}, {"Gomel", "Гомель"}, {"Mozyr", "Мозир"}, {"Voronezh", "Воронеж"},
    {"Rostov", "Ростов"}, {"Krasnodar", "Краснодар"}};

static const char *cityName(const City &c) {
    auto it = UKR.find(c.name);
    return it == UKR.end() ? c.name.c_str() : it->second;
}

static SDL_Color UA_C = {70, 130, 235, 255}, RU_C = {220, 70, 60, 255}, WHITE = {240, 240, 240, 255},
                 DIM = {170, 175, 185, 255}, GOLD = {240, 200, 70, 255};

static void text(TTF_Font *f, const std::string &s, int x, int y, SDL_Color c, bool shadow = false, bool center = false) {
    if (s.empty()) return;
    auto draw = [&](int ox, int oy, SDL_Color col) {
        SDL_Surface *surf = TTF_RenderUTF8_Blended(f, s.c_str(), col);
        if (!surf) return;
        SDL_Texture *t = SDL_CreateTextureFromSurface(R, surf);
        SDL_Rect d = {x + ox - (center ? surf->w / 2 : 0), y + oy, surf->w, surf->h};
        SDL_RenderCopy(R, t, nullptr, &d);
        SDL_DestroyTexture(t);
        SDL_FreeSurface(surf);
    };
    if (shadow) draw(1, 1, SDL_Color{0, 0, 0, 255});
    draw(0, 0, c);
}

static void rect(int x, int y, int w, int h, SDL_Color c, bool fill = true) {
    SDL_SetRenderDrawColor(R, c.r, c.g, c.b, c.a);
    SDL_Rect r = {x, y, w, h};
    if (fill) SDL_RenderFillRect(R, &r); else SDL_RenderDrawRect(R, &r);
}

static std::string dateStr(int day) { // day 0 = 24 Feb 2022
    long z = 738574 + day; // days since 0000-03-01 style epoch handled below
    // civil_from_days (Howard Hinnant), with 1970-01-01 = 0; 2022-02-24 = 19047
    z = 19047 + day + 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = unsigned(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long y = long(yoe) + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned d = doy - (153 * mp + 2) / 5 + 1, m = mp < 10 ? mp + 3 : mp - 9;
    if (m <= 2) y++;
    static const char *MN[] = {"", "січня", "лютого", "березня", "квітня", "травня", "червня", "липня", "серпня", "вересня", "жовтня", "листопада", "грудня"};
    char buf[64];
    snprintf(buf, sizeof buf, "%u %s %ld", d, MN[m], y);
    return buf;
}

struct Game {
    World w;
    int side = -1;
    bool started = false, paused = true, showSupply = false;
    int speed = 1;
    double acc = 0;
    std::vector<int> sel;
    bool dragging = false;
    int dx0 = 0, dy0 = 0, mx = 0, my = 0;

    void start(int s) {
        side = s; w.init(uint32_t(SDL_GetTicks()) | 1u, s);
        started = true; paused = true; speed = 1; sel.clear();
    }
};

static SDL_Color mix(SDL_Color a, SDL_Color b, float t) {
    return {Uint8(a.r + (b.r - a.r) * t), Uint8(a.g + (b.g - a.g) * t), Uint8(a.b + (b.b - a.b) * t), 255};
}

static void drawMap(Game &g) {
    World &w = g.w;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const Cell &c = w.at(x, y);
            SDL_Color col;
            if (c.t == SEA) col = {28, 52, 86, 255};
            else if (c.t == BLOCK) col = {46, 48, 52, 255};
            else {
                float sh = std::min(1.f, (c.rough - 1.f) * 2.2f);
                col = mix(SDL_Color{128, 138, 112, 255}, SDL_Color{84, 102, 76, 255}, sh);
                if (c.owner == UA) col = mix(col, UA_C, 0.55f);
                else if (c.owner == RU) col = mix(col, RU_C, 0.55f);
                if (c.cap > 0.05f) col = mix(col, c.owner == UA ? RU_C : UA_C, c.cap * 0.5f);
                if (g.showSupply && g.side >= 0) {
                    float s = w.sup[g.side][World::idx(x, y)];
                    col = mix(col, SDL_Color{255, 230, 80, 255}, s * 0.7f);
                }
            }
            rect(x * CELL, y * CELL, CELL, CELL, col);
            if (c.river) rect(x * CELL + 2, y * CELL + 2, CELL - 4, CELL - 4, SDL_Color{90, 170, 230, 255});
        }
    // front line: draw border segments between different owners
    SDL_SetRenderDrawColor(R, 15, 15, 20, 255);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const Cell &c = w.at(x, y);
            if (c.t != LAND || c.owner < 0) continue;
            if (x + 1 < W && w.at(x + 1, y).t == LAND && w.at(x + 1, y).owner >= 0 && w.at(x + 1, y).owner != c.owner)
                SDL_RenderDrawLine(R, (x + 1) * CELL, y * CELL, (x + 1) * CELL, (y + 1) * CELL);
            if (y + 1 < H && w.at(x, y + 1).t == LAND && w.at(x, y + 1).owner >= 0 && w.at(x, y + 1).owner != c.owner)
                SDL_RenderDrawLine(R, x * CELL, (y + 1) * CELL, (x + 1) * CELL, (y + 1) * CELL);
        }
    // cities
    for (const auto &c : w.cities) {
        if (c.rear && c.base) continue;
        int o = w.at(c.x, c.y).owner;
        int s = 4 + c.value;
        rect(c.x * CELL + CELL / 2 - s / 2, c.y * CELL + CELL / 2 - s / 2, s, s, WHITE);
        rect(c.x * CELL + CELL / 2 - s / 2 + 1, c.y * CELL + CELL / 2 - s / 2 + 1, s - 2, s - 2, o == UA ? UA_C : RU_C);
        if (c.value >= 2) text(F12, cityName(c), c.x * CELL + CELL / 2 + s / 2 + 2, c.y * CELL - 1, WHITE, true);
    }
    // brigades
    std::vector<int> stack(N, 0);
    for (const auto &b : w.brig) {
        int i = World::idx(b.x, b.y), k = stack[i]++;
        int bx = b.x * CELL + 1 + (k % 3) * 3, by = b.y * CELL + 2 + (k / 3) * 4 - (k % 3);
        SDL_Color sc = b.side == UA ? UA_C : RU_C;
        bool selected = std::find(g.sel.begin(), g.sel.end(), b.id) != g.sel.end();
        rect(bx - 1, by - 1, 12, 9, selected ? SDL_Color{255, 255, 255, 255} : SDL_Color{10, 10, 10, 255});
        rect(bx, by, 10, 7, b.fighting ? SDL_Color{255, 150, 40, 255} : sc);
        float f = std::max(0.f, std::min(1.f, b.men / b.maxMen));
        rect(bx, by + 5, int(10 * f), 2, SDL_Color{240, 240, 240, 255});
        if (selected && b.tx >= 0) {
            SDL_SetRenderDrawColor(R, 255, 255, 255, 255);
            SDL_RenderDrawLine(R, bx + 5, by + 3, b.tx * CELL + CELL / 2, b.ty * CELL + CELL / 2);
            rect(b.tx * CELL + 2, b.ty * CELL + 2, CELL - 4, CELL - 4, WHITE, false);
        }
    }
    if (g.dragging) {
        SDL_Rect r = {std::min(g.dx0, g.mx), std::min(g.dy0, g.my), std::abs(g.mx - g.dx0), std::abs(g.my - g.dy0)};
        SDL_SetRenderDrawColor(R, 255, 255, 255, 255);
        SDL_RenderDrawRect(R, &r);
    }
}

static void drawPanel(Game &g) {
    World &w = g.w;
    rect(MAPW, 0, PANEL, WIN_H, SDL_Color{24, 27, 33, 255});
    int x = MAPW + 14, y = 10;
    text(F20, "FRONTLINE 2022", x, y, GOLD); y += 30;
    text(F20, dateStr(w.day), x, y, WHITE); y += 26;
    const char *sp[] = {"Пауза", "x1", "x2", "x3", "x4"};
    text(F14, std::string("Швидкість: ") + (g.paused ? "Пауза" : sp[g.speed]), x, y, DIM); y += 26;
    Stats s = w.stats();
    text(F14, "Територія України", x, y, DIM); y += 20;
    int barW = PANEL - 28;
    rect(x, y, barW, 14, SDL_Color{50, 50, 56, 255});
    int uaw = s.total ? barW * s.cells[UA] / s.total : 0, ruw = s.total ? barW * s.cells[RU] / s.total : 0;
    rect(x, y, uaw, 14, UA_C); rect(x + barW - ruw, y, ruw, 14, RU_C); y += 18;
    char buf[160];
    snprintf(buf, sizeof buf, "ЗСУ %d%%        РФ %d%%", s.total ? 100 * s.cells[UA] / s.total : 0, s.total ? 100 * s.cells[RU] / s.total : 0);
    text(F14, buf, x, y, WHITE); y += 28;
    snprintf(buf, sizeof buf, "Бригад:   %d / %d", s.brigades[UA], s.brigades[RU]); text(F14, buf, x, y, WHITE); y += 20;
    snprintf(buf, sizeof buf, "Особовий склад: %ldк / %ldк", s.manpower[UA] / 1000, s.manpower[RU] / 1000); text(F14, buf, x, y, WHITE); y += 20;
    snprintf(buf, sizeof buf, "Втрати: %ldк / %ldк", s.lost[UA] / 1000, s.lost[RU] / 1000); text(F14, buf, x, y, WHITE); y += 20;
    if (w.kyivDays > 0) {
        snprintf(buf, sizeof buf, "Київ під контролем РФ: %d/30 днів", w.kyivDays);
        text(F14, buf, x, y, SDL_Color{255, 120, 100, 255});
    }
    y += 28;
    text(F14, g.side == UA ? "Ви: Україна (ЗСУ)" : g.side == RU ? "Ви: Росія" : "Режим спостерігача", x, y, g.side == UA ? UA_C : g.side == RU ? RU_C : DIM); y += 28;

    // selection info
    if (!g.sel.empty()) {
        Brigade *b = w.find(g.sel[0]);
        if (b) {
            snprintf(buf, sizeof buf, "Бригада %d  (вибрано: %zu)", b->num, g.sel.size());
            text(F14, buf, x, y, GOLD); y += 22;
            snprintf(buf, sizeof buf, "Люди: %d / %d", int(b->men), int(b->maxMen)); text(F12, buf, x, y, WHITE); y += 17;
            snprintf(buf, sizeof buf, "Організація: %d%%", int(b->org)); text(F12, buf, x, y, WHITE); y += 17;
            snprintf(buf, sizeof buf, "Постачання: %d%%", int(b->sup * 100)); text(F12, buf, x, y, WHITE); y += 17;
            snprintf(buf, sizeof buf, "Досвід: %.1f   Окопи: %d%%", b->exp, int(b->dug * 100)); text(F12, buf, x, y, WHITE); y += 17;
            text(F12, b->fighting ? "Стан: бій" : (b->tx >= 0 ? "Стан: рух" : "Стан: утримує позицію"), x, y, WHITE); y += 24;
        }
    }
    // hover info
    int cx = g.mx / CELL, cy = g.my / CELL;
    if (g.mx < MAPW && World::in(cx, cy)) {
        const Cell &c = w.at(cx, cy);
        if (c.t == LAND) {
            if (c.city >= 0) { text(F14, cityName(w.cities[c.city]), x, y, WHITE); y += 20; }
            snprintf(buf, sizeof buf, "Власник: %s   Захист: %.2f%s", c.owner == UA ? "ЗСУ" : "РФ", c.rough * (c.river ? 1.3f : 1.f), c.river ? " (річка)" : "");
            text(F12, buf, x, y, DIM); y += 17;
            if (g.side >= 0) { snprintf(buf, sizeof buf, "Постачання: %d%%", int(w.sup[g.side][World::idx(cx, cy)] * 100)); text(F12, buf, x, y, DIM); y += 17; }
        }
    }
    // help
    y = WIN_H - 190;
    const char *help[] = {"ЛКМ: вибір (можна рамкою)", "Shift+ЛКМ: додати до вибору", "ПКМ: наказ рухатись/атакувати", "H: утримувати позицію",
                          "Пробіл: пауза     1-4: швидкість", "S: показати постачання", "T: ШІ допомагає вам", "Ctrl+A: вибрати всі ваші", "Esc: вихід"};
    for (auto *h : help) { text(F12, h, x, y, DIM); y += 18; }
}

static void drawMenu(Game &g) {
    SDL_SetRenderDrawColor(R, 18, 20, 26, 255); SDL_RenderClear(R);
    int cx = WIN_W / 2;
    text(F32, "FRONTLINE 2022", cx, 150, GOLD, true, true);
    text(F14, "Стратегія в реальному часі з паузою. Фронт, бригади, постачання.", cx, 205, DIM, false, true);
    text(F20, "1 — грати за Україну (оборона)", cx, 300, UA_C, false, true);
    text(F20, "2 — грати за Росію (наступ)", cx, 345, RU_C, false, true);
    text(F20, "3 — спостерігати (ШІ проти ШІ)", cx, 390, WHITE, false, true);
    text(F14, "Оригінальний код і графіка. Карта наближена, ~17 км на клітинку.", cx, 520, DIM, false, true);
    (void)g;
}

static void render(Game &g) {
    if (!g.started) { drawMenu(g); return; }
    SDL_SetRenderDrawColor(R, 0, 0, 0, 255); SDL_RenderClear(R);
    drawMap(g);
    drawPanel(g);
    if (g.w.winner != NONE_YET) {
        rect(MAPW / 2 - 260, 260, 520, 150, SDL_Color{10, 10, 14, 235});
        const char *msg = g.w.winner == WIN_UA ? "ПЕРЕМОГА УКРАЇНИ" : g.w.winner == WIN_RU ? "ПЕРЕМОГА РОСІЇ" : "НІЧИЯ";
        text(F32, msg, MAPW / 2, 285, GOLD, true, true);
        text(F14, "Esc — вихід", MAPW / 2, 350, DIM, false, true);
    }
}

static std::vector<int> brigadesInCell(Game &g, int cx, int cy) {
    std::vector<int> v;
    for (const auto &b : g.w.brig) if (b.side == g.side && b.x == cx && b.y == cy) v.push_back(b.id);
    return v;
}

int main(int argc, char **argv) {
    int shotDays = -1, shotSide = -1;
    const char *shotFile = nullptr;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) shotFile = argv[++i];
        else if (!strcmp(argv[i], "--days") && i + 1 < argc) shotDays = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--side") && i + 1 < argc) shotSide = atoi(argv[++i]);
    }
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    SDL_Window *win = SDL_CreateWindow("Frontline 2022", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, 0);
    R = SDL_CreateRenderer(win, -1, shotFile ? SDL_RENDERER_SOFTWARE : SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!R) R = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
    std::string base = SDL_GetBasePath() ? SDL_GetBasePath() : "./";
    std::string fp = base + "assets/DejaVuSans.ttf";
    if (!TTF_OpenFont(fp.c_str(), 12)) fp = "assets/DejaVuSans.ttf";
    F12 = TTF_OpenFont(fp.c_str(), 12); F14 = TTF_OpenFont(fp.c_str(), 14);
    F20 = TTF_OpenFont(fp.c_str(), 20); F32 = TTF_OpenFont(fp.c_str(), 32);
    if (!F12) { fprintf(stderr, "font not found: %s\n", TTF_GetError()); return 1; }

    Game g;
    if (shotFile) {
        g.start(shotSide < 0 ? UA : shotSide);
        g.w.assist = true;
        for (auto &b : g.w.brig) b.ai = true;
        while (g.w.day < shotDays && g.w.winner == NONE_YET) g.w.step();
        g.paused = false;
        if (!g.sel.size() && !g.w.brig.empty()) for (auto &b : g.w.brig) if (b.side == g.side) { g.sel.push_back(b.id); break; }
        render(g);
        SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, WIN_W, WIN_H, 32, SDL_PIXELFORMAT_ARGB8888);
        SDL_RenderReadPixels(R, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
        SDL_SaveBMP(s, shotFile);
        return 0;
    }

    bool run = true;
    Uint32 last = SDL_GetTicks();
    while (run) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) run = false;
            else if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;
                if (k == SDLK_ESCAPE) run = false;
                if (!g.started) {
                    if (k == SDLK_1) g.start(UA); else if (k == SDLK_2) g.start(RU); else if (k == SDLK_3) g.start(-1);
                    if (g.started && g.side < 0) { g.paused = false; }
                } else {
                    if (k == SDLK_SPACE) g.paused = !g.paused;
                    else if (k >= SDLK_1 && k <= SDLK_4) { g.speed = int(k - SDLK_1) + 1; g.paused = false; }
                    else if (k == SDLK_s) g.showSupply = !g.showSupply;
                    else if (k == SDLK_t) { g.w.assist = !g.w.assist; for (auto &b : g.w.brig) if (b.side == g.side) b.ai = g.w.assist; }
                    else if (k == SDLK_h) for (int id : g.sel) if (Brigade *b = g.w.find(id)) b->tx = b->ty = -1;
                    else if (k == SDLK_a && (SDL_GetModState() & KMOD_CTRL)) {
                        g.sel.clear(); for (auto &b : g.w.brig) if (b.side == g.side) g.sel.push_back(b.id);
                    }
                }
            } else if (e.type == SDL_MOUSEMOTION) { g.mx = e.motion.x; g.my = e.motion.y; }
            else if (e.type == SDL_MOUSEBUTTONDOWN && g.started) {
                g.mx = e.button.x; g.my = e.button.y;
                if (e.button.button == SDL_BUTTON_LEFT && g.mx < MAPW) { g.dragging = true; g.dx0 = g.mx; g.dy0 = g.my; }
                else if (e.button.button == SDL_BUTTON_RIGHT && g.mx < MAPW) {
                    int cx = g.mx / CELL, cy = g.my / CELL;
                    for (int id : g.sel) g.w.order(id, cx, cy);
                }
            } else if (e.type == SDL_MOUSEBUTTONUP && g.started && e.button.button == SDL_BUTTON_LEFT && g.dragging) {
                g.dragging = false;
                bool add = SDL_GetModState() & KMOD_SHIFT;
                if (!add) g.sel.clear();
                if (std::abs(g.mx - g.dx0) < 4 && std::abs(g.my - g.dy0) < 4) {
                    for (int id : brigadesInCell(g, g.mx / CELL, g.my / CELL))
                        if (std::find(g.sel.begin(), g.sel.end(), id) == g.sel.end()) g.sel.push_back(id);
                } else {
                    int x0 = std::min(g.dx0, g.mx) / CELL, x1 = std::max(g.dx0, g.mx) / CELL;
                    int y0 = std::min(g.dy0, g.my) / CELL, y1 = std::max(g.dy0, g.my) / CELL;
                    for (auto &b : g.w.brig)
                        if (b.side == g.side && b.x >= x0 && b.x <= x1 && b.y >= y0 && b.y <= y1 &&
                            std::find(g.sel.begin(), g.sel.end(), b.id) == g.sel.end()) g.sel.push_back(b.id);
                }
            }
        }
        Uint32 now = SDL_GetTicks();
        double dt = (now - last) / 1000.0; last = now;
        if (g.started && !g.paused && g.w.winner == NONE_YET) {
            static const double TPS[] = {0, 3, 8, 20, 60};
            g.acc += dt * TPS[g.speed];
            int n = 0;
            while (g.acc >= 1.0 && n < 30) { g.w.step(); g.acc -= 1.0; n++; }
            if (g.acc > 30) g.acc = 0;
            // drop dead brigades from selection
            g.sel.erase(std::remove_if(g.sel.begin(), g.sel.end(), [&](int id) { return !g.w.find(id); }), g.sel.end());
        }
        render(g);
        SDL_RenderPresent(R);
    }
    SDL_Quit();
    return 0;
}
