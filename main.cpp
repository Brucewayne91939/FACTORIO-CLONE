// ============================================================================
//  MiniFactorio — a compact Factorio-like factory game
//  C++17 + SFML 2.5/2.6 (graphics, window, system)
//
//  Controls:
//      WASD ............ move
//      1..4 ............ select belt / miner / furnace / assembler
//      R ............... rotate
//      LMB (drag) ...... place
//      RMB (drag) ...... remove
//      Mouse wheel ..... zoom
// ============================================================================

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

// ------------------------------------------------------------------ config --
constexpr int   TILE         = 32;
constexpr int   WORLD_W      = 160;
constexpr int   WORLD_H      = 160;
constexpr int   BELT_SLOTS   = 4;
constexpr float BELT_STEP_T  = 0.10f;   // seconds for an item to advance one slot
constexpr float MINE_TIME    = 0.85f;   // seconds per ore
constexpr float SMELT_TIME   = 1.30f;   // seconds per plate
constexpr float CRAFT_TIME   = 1.00f;   // seconds per gear (2 plates)
constexpr int   MACHINE_CAP  = 8;       // max items buffered in a machine
constexpr float PLAYER_SPEED = 220.f;

// ------------------------------------------------------------------- items --
enum ItemId : uint8_t {
    ITEM_NONE = 0,
    ITEM_IRON_ORE,
    ITEM_COPPER_ORE,
    ITEM_IRON_PLATE,
    ITEM_COPPER_PLATE,
    ITEM_GEAR,
    ITEM_COUNT
};

struct ItemDef { const char* name; sf::Color col; };
static const ItemDef ITEM_DEFS[ITEM_COUNT] = {
    { "NONE",         sf::Color(  0,   0,   0) },
    { "IRON ORE",     sf::Color(150, 150, 175) },
    { "COPPER ORE",   sf::Color(200, 120,  60) },
    { "IRON PLATE",   sf::Color(205, 205, 215) },
    { "COPPER PLATE", sf::Color(235, 150,  80) },
    { "GEAR",         sf::Color(170, 170, 180) },
};

// --------------------------------------------------------------- buildings --
enum BldId : uint8_t {
    BLD_NONE = 0,
    BLD_BELT,
    BLD_MINER,
    BLD_FURNACE,
    BLD_ASSEMBLER,
    BLD_COUNT
};

static const char* BLD_NAMES[BLD_COUNT] = {
    "NONE", "BELT", "MINER", "FURNACE", "ASSEMBLER"
};

// ------------------------------------------------------------------ grid ----
enum Dir { DIR_N = 0, DIR_E = 1, DIR_S = 2, DIR_W = 3 };

struct IVec2 { int x, y; };
static const IVec2 DIRV[4] = { {0,-1}, {1,0}, {0,1}, {-1,0} };

inline bool inBounds(int x, int y) {
    return x >= 0 && y >= 0 && x < WORLD_W && y < WORLD_H;
}

// One world cell holds terrain + building + its state. Flat and cache friendly.
struct Cell {
    uint8_t terrain  = 0;                       // 0 grass, 1 water
    uint8_t ore      = 0;                       // 0 none, 1 iron, 2 copper
    uint8_t building = BLD_NONE;
    uint8_t dir      = DIR_E;

    std::array<uint8_t, BELT_SLOTS> belt{};     // belt contents (item ids)

    float   progress = 0.f;                     // crafting / mining progress 0..1
    uint8_t inItem   = ITEM_NONE;               // machine input buffer
    int     inCount  = 0;
    uint8_t outItem  = ITEM_NONE;               // machine output buffer
    int     outCount = 0;
};

// ============================================================================
//  3x5 bitmap font (so we need no external font file)
// ============================================================================
static const uint8_t FONT[42][5] = {
    {0b111,0b101,0b101,0b101,0b111}, // 0
    {0b010,0b110,0b010,0b010,0b111}, // 1
    {0b111,0b001,0b111,0b100,0b111}, // 2
    {0b111,0b001,0b111,0b001,0b111}, // 3
    {0b101,0b101,0b111,0b001,0b001}, // 4
    {0b111,0b100,0b111,0b001,0b111}, // 5
    {0b111,0b100,0b111,0b101,0b111}, // 6
    {0b111,0b001,0b001,0b001,0b001}, // 7
    {0b111,0b101,0b111,0b101,0b111}, // 8
    {0b111,0b101,0b111,0b001,0b111}, // 9
    {0b111,0b101,0b111,0b101,0b101}, // A
    {0b110,0b101,0b110,0b101,0b110}, // B
    {0b111,0b100,0b100,0b100,0b111}, // C
    {0b110,0b101,0b101,0b101,0b110}, // D
    {0b111,0b100,0b111,0b100,0b111}, // E
    {0b111,0b100,0b111,0b100,0b100}, // F
    {0b111,0b100,0b101,0b101,0b111}, // G
    {0b101,0b101,0b111,0b101,0b101}, // H
    {0b111,0b010,0b010,0b010,0b111}, // I
    {0b001,0b001,0b001,0b101,0b111}, // J
    {0b101,0b101,0b110,0b101,0b101}, // K
    {0b100,0b100,0b100,0b100,0b111}, // L
    {0b101,0b111,0b111,0b101,0b101}, // M
    {0b110,0b111,0b101,0b101,0b101}, // N
    {0b111,0b101,0b101,0b101,0b111}, // O
    {0b111,0b101,0b111,0b100,0b100}, // P
    {0b111,0b101,0b101,0b111,0b001}, // Q
    {0b111,0b101,0b111,0b110,0b101}, // R
    {0b111,0b100,0b111,0b001,0b111}, // S
    {0b111,0b010,0b010,0b010,0b010}, // T
    {0b101,0b101,0b101,0b101,0b111}, // U
    {0b101,0b101,0b101,0b101,0b010}, // V
    {0b101,0b101,0b111,0b111,0b101}, // W
    {0b101,0b101,0b010,0b101,0b101}, // X
    {0b101,0b101,0b010,0b010,0b010}, // Y
    {0b111,0b001,0b010,0b100,0b111}, // Z
    {0b000,0b000,0b000,0b000,0b000}, // space
    {0b000,0b010,0b000,0b010,0b000}, // :
    {0b000,0b000,0b000,0b000,0b010}, // .
    {0b000,0b000,0b111,0b000,0b000}, // -
    {0b001,0b001,0b010,0b100,0b100}, // /
    {0b010,0b010,0b010,0b000,0b010}, // !
};

static int glyphIndex(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'Z') return 10 + (ch - 'A');
    if (ch >= 'a' && ch <= 'z') return 10 + (ch - 'a');
    switch (ch) {
        case ' ': return 36;
        case ':': return 37;
        case '.': return 38;
        case '-': return 39;
        case '/': return 40;
        case '!': return 41;
    }
    return -1;
}

// ============================================================================
//  Game
// ============================================================================
class Game {
public:
    Game();
    void run();

private:
    // ---- window / view
    sf::RenderWindow window;
    sf::View         view;
    sf::Vector2f     baseSize{ 1280.f, 720.f };
    float            zoom = 1.f;

    // ---- world
    std::vector<Cell> cells;
    std::mt19937      rng;

    // ---- player
    sf::Vector2f playerPos;
    sf::CircleShape playerShape;

    // ---- interaction
    uint8_t  selected  = BLD_BELT;
    uint8_t  ghostDir  = DIR_E;
    int      hoverX    = 0, hoverY = 0;
    float    beltTimer = 0.f;
    bool     rLatch    = false;

    // ---------------------------------------------------------------- world
    Cell&       at(int x, int y)       { return cells[y * WORLD_W + x]; }
    const Cell& at(int x, int y) const { return cells[y * WORLD_W + x]; }

    void generateWorld();

    // --------------------------------------------------------------- build
    bool canPlace(int x, int y, uint8_t bld) const;
    void placeBuilding(int x, int y);
    void removeBuilding(int x, int y);

    // ------------------------------------------------------------ machines
    static bool machineCanAccept(const Cell& c, uint8_t item);
    static void machineAccept(Cell& c, uint8_t item);
    static void addOutput(Cell& c, uint8_t item);
    void machineOutput(int x, int y);
    void updateMachine(int x, int y, float dt);

    // --------------------------------------------------------------- belts
    void updateBelts();

    // --------------------------------------------------------------- frame
    void handleEvents();
    void update(float dt);
    void render();

    // ---------------------------------------------------------------- draw
    void drawWorld();
    void drawBuildingShape(sf::RenderTarget& rt, uint8_t bld, uint8_t dir,
                           sf::Vector2f center, float size, uint8_t alpha = 255);
    void drawText(sf::RenderTarget& rt, const std::string& s,
                  float x, float y, float px, sf::Color col);
    void drawUI();
};

// ----------------------------------------------------------------------------
Game::Game() : rng(std::random_device{}()) {
    window.create(sf::VideoMode(1280, 720), "MiniFactorio");
    window.setFramerateLimit(60);

    cells.resize(WORLD_W * WORLD_H);

    playerPos = { WORLD_W * TILE * 0.5f, WORLD_H * TILE * 0.5f };
    playerShape.setRadius(11.f);
    playerShape.setOrigin(11.f, 11.f);
    playerShape.setFillColor(sf::Color(245, 205, 90));
    playerShape.setOutlineThickness(2.f);
    playerShape.setOutlineColor(sf::Color(60, 45, 10));

    baseSize = { 1280.f, 720.f };
    view = sf::View(sf::FloatRect(0.f, 0.f, baseSize.x, baseSize.y));
    view.setCenter(playerPos);

    generateWorld();
}

// ---------------------------------------------------------------- world gen
void Game::generateWorld() {
    std::uniform_int_distribution<int> rx(0, WORLD_W - 1), ry(0, WORLD_H - 1);
    std::uniform_real_distribution<float> rr(3.0f, 8.5f);

    auto blob = [&](int cx, int cy, float r, uint8_t oreType, bool water) {
        int R = static_cast<int>(std::ceil(r));
        for (int y = cy - R; y <= cy + R; ++y) {
            for (int x = cx - R; x <= cx + R; ++x) {
                if (!inBounds(x, y)) continue;
                float dx = float(x - cx), dy = float(y - cy);
                if (dx * dx + dy * dy > r * r) continue;
                Cell& c = at(x, y);
                if (water) c.terrain = 1;
                else if (c.terrain == 0) c.ore = oreType;
            }
        }
    };

    // iron patches
    for (int i = 0; i < 48; ++i) blob(rx(rng), ry(rng), rr(rng), 1, false);
    // copper patches
    for (int i = 0; i < 36; ++i) blob(rx(rng), ry(rng), rr(rng), 2, false);
    // ponds
    for (int i = 0; i < 14; ++i) blob(rx(rng), ry(rng), rr(rng) * 0.8f, 0, true);

    // guarantee starter resources near the spawn point
    int cx = WORLD_W / 2, cy = WORLD_H / 2;
    blob(cx + 9,  cy,      6.5f, 1, false);
    blob(cx - 9,  cy + 5,  6.5f, 2, false);

    // clear a safe starting area
    for (int y = cy - 6; y <= cy + 6; ++y)
        for (int x = cx - 6; x <= cx + 6; ++x)
            if (inBounds(x, y)) at(x, y).terrain = 0;
}

// ------------------------------------------------------------------ placing
bool Game::canPlace(int x, int y, uint8_t bld) const {
    if (!inBounds(x, y)) return false;
    const Cell& c = at(x, y);
    if (c.terrain == 1)      return false;
    if (c.building != BLD_NONE) return false;
    if (bld == BLD_MINER && c.ore == 0) return false;
    return true;
}

void Game::placeBuilding(int x, int y) {
    if (!canPlace(x, y, selected)) return;
    Cell& c = at(x, y);
    c.building = selected;
    c.dir      = ghostDir;
    c.belt.fill(ITEM_NONE);
    c.progress = 0.f;
    c.inItem = c.outItem = ITEM_NONE;
    c.inCount = c.outCount = 0;
}

void Game::removeBuilding(int x, int y) {
    if (!inBounds(x, y)) return;
    Cell& c = at(x, y);
    c.building = BLD_NONE;
    c.belt.fill(ITEM_NONE);
    c.progress = 0.f;
    c.inItem = c.outItem = ITEM_NONE;
    c.inCount = c.outCount = 0;
}

// ---------------------------------------------------------------- machines
bool Game::machineCanAccept(const Cell& c, uint8_t item) {
    switch (c.building) {
        case BLD_FURNACE:
            if (item != ITEM_IRON_ORE && item != ITEM_COPPER_ORE) return false;
            if (c.inCount > 0 && c.inItem != item) return false;
            return c.inCount < MACHINE_CAP;
        case BLD_ASSEMBLER:
            if (item != ITEM_IRON_PLATE) return false;
            return c.inCount < MACHINE_CAP;
        default:
            return false;
    }
}

void Game::machineAccept(Cell& c, uint8_t item) {
    if (c.inCount == 0) c.inItem = item;
    ++c.inCount;
}

void Game::addOutput(Cell& c, uint8_t item) {
    if (c.outCount == 0) c.outItem = item;
    ++c.outCount;
}

// Push one buffered output item to the tile this machine faces.
void Game::machineOutput(int x, int y) {
    Cell& c = at(x, y);
    if (c.outCount <= 0) return;

    IVec2 d = DIRV[c.dir];
    int nx = x + d.x, ny = y + d.y;
    if (!inBounds(nx, ny)) return;

    Cell& t = at(nx, ny);

    if (t.building == BLD_BELT) {
        // Do not feed a belt that points straight back into us (would loop).
        IVec2 td = DIRV[t.dir];
        if (nx + td.x == x && ny + td.y == y) return;
        if (t.belt[0] != ITEM_NONE) return;
        t.belt[0] = c.outItem;
        --c.outCount;
        if (c.outCount == 0) c.outItem = ITEM_NONE;
    } else if (machineCanAccept(t, c.outItem)) {
        machineAccept(t, c.outItem);
        --c.outCount;
        if (c.outCount == 0) c.outItem = ITEM_NONE;
    }
}

void Game::updateMachine(int x, int y, float dt) {
    Cell& c = at(x, y);

    machineOutput(x, y);

    switch (c.building) {
        case BLD_MINER: {
            if (c.ore == 0) { c.progress = 0.f; break; }
            if (c.outCount >= MACHINE_CAP) break;
            c.progress += dt / MINE_TIME;
            if (c.progress >= 1.f) {
                c.progress -= 1.f;
                addOutput(c, c.ore == 1 ? ITEM_IRON_ORE : ITEM_COPPER_ORE);
            }
            break;
        }
        case BLD_FURNACE: {
            if (c.inCount <= 0) { c.progress = 0.f; break; }
            if (c.outCount >= MACHINE_CAP) break;
            c.progress += dt / SMELT_TIME;
            if (c.progress >= 1.f) {
                c.progress -= 1.f;
                uint8_t out = (c.inItem == ITEM_IRON_ORE) ? ITEM_IRON_PLATE
                                                          : ITEM_COPPER_PLATE;
                --c.inCount;
                if (c.inCount == 0) c.inItem = ITEM_NONE;
                addOutput(c, out);
            }
            break;
        }
        case BLD_ASSEMBLER: {
            if (c.inCount < 2) { c.progress = 0.f; break; }
            if (c.outCount >= MACHINE_CAP) break;
            c.progress += dt / CRAFT_TIME;
            if (c.progress >= 1.f) {
                c.progress -= 1.f;
                c.inCount -= 2;
                if (c.inCount == 0) c.inItem = ITEM_NONE;
                addOutput(c, ITEM_GEAR);
            }
            break;
        }
        default:
            break;
    }
}

// ------------------------------------------------------------------- belts
void Game::updateBelts() {
    // Phase 1 — shift items one slot toward the front of every belt.
    for (int y = 0; y < WORLD_H; ++y) {
        for (int x = 0; x < WORLD_W; ++x) {
            Cell& c = at(x, y);
            if (c.building != BLD_BELT) continue;
            for (int i = BELT_SLOTS - 1; i > 0; --i) {
                if (c.belt[i] == ITEM_NONE && c.belt[i - 1] != ITEM_NONE) {
                    c.belt[i]     = c.belt[i - 1];
                    c.belt[i - 1] = ITEM_NONE;
                }
            }
        }
    }

    // Phase 2 — hand the front item over to the next tile.
    for (int y = 0; y < WORLD_H; ++y) {
        for (int x = 0; x < WORLD_W; ++x) {
            Cell& c = at(x, y);
            if (c.building != BLD_BELT) continue;

            uint8_t item = c.belt[BELT_SLOTS - 1];
            if (item == ITEM_NONE) continue;

            IVec2 d = DIRV[c.dir];
            int nx = x + d.x, ny = y + d.y;
            if (!inBounds(nx, ny)) continue;

            Cell& t = at(nx, ny);

            if (t.building == BLD_BELT) {
                IVec2 td = DIRV[t.dir];
                if (nx + td.x == x && ny + td.y == y) continue; // facing each other
                if (t.belt[0] != ITEM_NONE) continue;
                t.belt[0] = item;
                c.belt[BELT_SLOTS - 1] = ITEM_NONE;
            } else if (machineCanAccept(t, item)) {
                machineAccept(t, item);
                c.belt[BELT_SLOTS - 1] = ITEM_NONE;
            }
        }
    }
}

// ------------------------------------------------------------------ events
void Game::handleEvents() {
    sf::Event event;
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            window.close();
        } else if (event.type == sf::Event::Resized) {
            baseSize = { float(event.size.width), float(event.size.height) };
            view.setSize(baseSize * zoom);
        } else if (event.type == sf::Event::MouseWheelScrolled) {
            if (event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel) {
                float f = (event.mouseWheelScroll.delta > 0) ? 0.88f : 1.14f;
                zoom = std::clamp(zoom * f, 0.45f, 3.0f);
                view.setSize(baseSize * zoom);
            }
        }
    }
}

// ------------------------------------------------------------------ update
void Game::update(float dt) {
    // --- player movement
    sf::Vector2f mv(0.f, 0.f);
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) mv.y -= 1.f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) mv.y += 1.f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) mv.x -= 1.f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) mv.x += 1.f;
    if (mv.x != 0.f || mv.y != 0.f) {
        float len = std::sqrt(mv.x * mv.x + mv.y * mv.y);
        mv /= len;
        playerPos += mv * PLAYER_SPEED * dt;
    }
    playerPos.x = std::clamp(playerPos.x, 16.f, float(WORLD_W * TILE) - 16.f);
    playerPos.y = std::clamp(playerPos.y, 16.f, float(WORLD_H * TILE) - 16.f);

    view.setCenter(playerPos);
    window.setView(view);

    // --- hotbar selection
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Num1)) selected = BLD_BELT;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Num2)) selected = BLD_MINER;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Num3)) selected = BLD_FURNACE;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Num4)) selected = BLD_ASSEMBLER;

    // --- mouse -> grid
    sf::Vector2f world = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    hoverX = static_cast<int>(std::floor(world.x / TILE));
    hoverY = static_cast<int>(std::floor(world.y / TILE));

    // --- rotate (edge triggered)
    bool rNow = sf::Keyboard::isKeyPressed(sf::Keyboard::R);
    if (rNow && !rLatch) {
        ghostDir = (ghostDir + 1) & 3;
        if (inBounds(hoverX, hoverY)) {
            Cell& c = at(hoverX, hoverY);
            if (c.building != BLD_NONE) c.dir = (c.dir + 1) & 3;
        }
    }
    rLatch = rNow;

    // --- build / remove (drag friendly)
    if (sf::Mouse::isButtonPressed(sf::Mouse::Left))  placeBuilding(hoverX, hoverY);
    if (sf::Mouse::isButtonPressed(sf::Mouse::Right)) removeBuilding(hoverX, hoverY);

    // --- belts
    beltTimer += dt;
    while (beltTimer >= BELT_STEP_T) {
        beltTimer -= BELT_STEP_T;
        updateBelts();
    }

    // --- machines
    for (int y = 0; y < WORLD_H; ++y)
        for (int x = 0; x < WORLD_W; ++x)
            if (at(x, y).building >= BLD_MINER) updateMachine(x, y, dt);
}

// ------------------------------------------------------------------ render
void Game::drawBuildingShape(sf::RenderTarget& rt, uint8_t bld, uint8_t dir,
                             sf::Vector2f c, float size, uint8_t alpha) {
    sf::Color base;
    switch (bld) {
        case BLD_BELT:      base = sf::Color( 78,  80,  92); break;
        case BLD_MINER:     base = sf::Color(185, 115,  45); break;
        case BLD_FURNACE:   base = sf::Color(125,  62,  55); break;
        case BLD_ASSEMBLER: base = sf::Color( 72, 112, 155); break;
        default: return;
    }
    base.a = alpha;

    sf::RectangleShape body({ size, size });
    body.setOrigin(size * 0.5f, size * 0.5f);
    body.setPosition(c);
    body.setFillColor(base);
    body.setOutlineThickness(2.f);
    body.setOutlineColor(sf::Color(18, 18, 22, alpha));
    rt.draw(body);

    const float rot = float(dir) * 90.f;

    if (bld == BLD_BELT) {
        sf::ConvexShape tri(3);
        float s = size * 0.22f;
        tri.setPoint(0, { 0.f,       -s        });
        tri.setPoint(1, { s * 0.9f,   s * 0.8f });
        tri.setPoint(2, { -s * 0.9f,  s * 0.8f });
        tri.setPosition(c);
        tri.setRotation(rot);
        tri.setFillColor(sf::Color(225, 225, 235, alpha));
        rt.draw(tri);

    } else if (bld == BLD_MINER) {
        sf::ConvexShape drill(3);
        float s = size * 0.30f;
        drill.setPoint(0, { 0.f,  s        });
        drill.setPoint(1, { -s,  -s * 0.6f });
        drill.setPoint(2, {  s,  -s * 0.6f });
        drill.setPosition(c);
        drill.setFillColor(sf::Color(245, 245, 250, alpha));
        rt.draw(drill);

    } else if (bld == BLD_FURNACE) {
        float r = size * 0.22f;
        sf::CircleShape glow(r);
        glow.setOrigin(r, r);
        glow.setPosition(c);
        glow.setFillColor(sf::Color(255, 150, 45, alpha));
        rt.draw(glow);

    } else if (bld == BLD_ASSEMBLER) {
        float r = size * 0.28f;
        sf::CircleShape gear(r);
        gear.setOrigin(r, r);
        gear.setPosition(c);
        gear.setFillColor(sf::Color(232, 232, 242, alpha));
        rt.draw(gear);

        float h = size * 0.11f;
        sf::CircleShape hole(h);
        hole.setOrigin(h, h);
        hole.setPosition(c);
        hole.setFillColor(base);
        rt.draw(hole);
    }
}

void Game::drawText(sf::RenderTarget& rt, const std::string& s,
                    float x, float y, float px, sf::Color col) {
    float cx = x;
    sf::RectangleShape q({ px, px });
    q.setFillColor(col);
    for (char ch : s) {
        int gi = glyphIndex(ch);
        if (gi >= 0) {
            for (int r = 0; r < 5; ++r) {
                for (int k = 0; k < 3; ++k) {
                    if (FONT[gi][r] & (4 >> k)) {
                        q.setPosition(cx + k * px, y + r * px);
                        rt.draw(q);
                    }
                }
            }
        }
        cx += 4.f * px;
    }
}

void Game::drawWorld() {
    // visible tile range
    sf::Vector2f tl = window.mapPixelToCoords(sf::Vector2i(0, 0));
    sf::Vector2f br = window.mapPixelToCoords(
        sf::Vector2i(int(window.getSize().x), int(window.getSize().y)));

    int x0 = std::max(0, int(tl.x / TILE) - 1);
    int y0 = std::max(0, int(tl.y / TILE) - 1);
    int x1 = std::min(WORLD_W - 1, int(br.x / TILE) + 1);
    int y1 = std::min(WORLD_H - 1, int(br.y / TILE) + 1);

    sf::RectangleShape rect({ float(TILE), float(TILE) });
    sf::CircleShape    blob(float(TILE) * 0.13f);

    // --- terrain + ore
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const Cell& c = at(x, y);
            float px = float(x * TILE), py = float(y * TILE);

            sf::Color ground = (c.terrain == 1) ? sf::Color(38, 82, 150)
                                                : sf::Color(66, 104, 58);
            if (((x + y) & 1) == 0) {
                ground.r = uint8_t(std::min(255, ground.r + 6));
                ground.g = uint8_t(std::min(255, ground.g + 6));
                ground.b = uint8_t(std::min(255, ground.b + 6));
            }
            rect.setPosition(px, py);
            rect.setFillColor(ground);
            window.draw(rect);

            if (c.ore != 0) {
                sf::Color oc = (c.ore == 1) ? sf::Color(150, 150, 178)
                                            : sf::Color(205, 125, 60);
                blob.setFillColor(oc);
                blob.setRadius(TILE * 0.11f);
                for (int k = 0; k < 4; ++k) {
                    float ox = ((k * 7 + x * 13) % 5) * 0.14f + 0.14f;
                    float oy = ((k * 5 + y * 11) % 5) * 0.14f + 0.14f;
                    blob.setPosition(px + ox * TILE, py + oy * TILE);
                    window.draw(blob);
                }
            }
        }
    }

    // --- buildings
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const Cell& c = at(x, y);
            if (c.building == BLD_NONE) continue;

            sf::Vector2f center(x * TILE + TILE * 0.5f, y * TILE + TILE * 0.5f);
            drawBuildingShape(window, c.building, c.dir, center, float(TILE) - 3.f);

            if (c.building >= BLD_MINER && c.progress > 0.01f) {
                sf::RectangleShape bar({ (TILE - 10.f) * c.progress, 3.f });
                bar.setPosition(x * TILE + 5.f, y * TILE + TILE - 6.f);
                bar.setFillColor(sf::Color(250, 220, 90));
                window.draw(bar);
            }
        }
    }

    // --- items riding on belts
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const Cell& c = at(x, y);
            if (c.building != BLD_BELT) continue;

            sf::Vector2f center(x * TILE + TILE * 0.5f, y * TILE + TILE * 0.5f);
            sf::Vector2f dv(float(DIRV[c.dir].x), float(DIRV[c.dir].y));

            for (int i = 0; i < BELT_SLOTS; ++i) {
                uint8_t item = c.belt[i];
                if (item == ITEM_NONE) continue;

                float t = (i + 0.5f) / float(BELT_SLOTS);
                sf::Vector2f p = center + dv * ((t - 0.5f) * TILE);

                float s = TILE * 0.30f;
                sf::RectangleShape box({ s, s });
                box.setOrigin(s * 0.5f, s * 0.5f);
                box.setPosition(p);
                box.setFillColor(ITEM_DEFS[item].col);
                box.setOutlineThickness(1.f);
                box.setOutlineColor(sf::Color(20, 20, 20));
                window.draw(box);
            }
        }
    }

    // --- ghost preview
    if (inBounds(hoverX, hoverY)) {
        bool ok = canPlace(hoverX, hoverY, selected);
        sf::RectangleShape hl({ float(TILE), float(TILE) });
        hl.setPosition(float(hoverX * TILE), float(hoverY * TILE));
        hl.setFillColor(ok ? sf::Color(120, 255, 120, 45)
                           : sf::Color(255, 80, 80, 55));
        hl.setOutlineThickness(2.f);
        hl.setOutlineColor(ok ? sf::Color(150, 255, 150, 200)
                              : sf::Color(255, 110, 110, 200));
        window.draw(hl);

        if (ok) {
            sf::Vector2f center(hoverX * TILE + TILE * 0.5f,
                                hoverY * TILE + TILE * 0.5f);
            drawBuildingShape(window, selected, ghostDir, center,
                              float(TILE) - 3.f, 130);
        }
    }

    // --- player
    playerShape.setPosition(playerPos);
    window.draw(playerShape);
}

void Game::drawUI() {
    window.setView(window.getDefaultView());

    const float W = float(window.getSize().x);
    const float H = float(window.getSize().y);

    drawText(window, "WASD MOVE   LMB BUILD   RMB REMOVE   R ROTATE   WHEEL ZOOM",
             16.f, 16.f, 3.f, sf::Color(235, 235, 240));
    drawText(window, "BELT FEEDS MACHINES  MINER MAKES ORE  FURNACE SMELTS  ASSEMBLER MAKES GEARS",
             16.f, 36.f, 3.f, sf::Color(180, 190, 175));

    const int   N       = 4;
    const float slot    = 58.f;
    const float pad     = 8.f;
    const float totalW  = N * slot + (N - 1) * pad;
    const float x0      = (W - totalW) * 0.5f;
    const float y0      = H - slot - 18.f;

    for (int i = 0; i < N; ++i) {
        uint8_t bld = uint8_t(BLD_BELT + i);
        float sx = x0 + i * (slot + pad);

        sf::RectangleShape box({ slot, slot });
        box.setPosition(sx, y0);
        box.setFillColor(selected == bld ? sf::Color(88, 92, 112)
                                         : sf::Color(46, 48, 58));
        box.setOutlineThickness(2.f);
        box.setOutlineColor(selected == bld ? sf::Color(245, 220, 110)
                                            : sf::Color(22, 22, 28));
        window.draw(box);

        drawBuildingShape(window, bld, DIR_E,
                          { sx + slot * 0.5f, y0 + slot * 0.5f - 4.f },
                          slot * 0.55f);

        drawText(window, std::to_string(i + 1),
                 sx + 6.f, y0 + slot - 14.f, 2.f, sf::Color(230, 230, 235));
    }

    std::string name = BLD_NAMES[selected];
    float tw = float(name.size()) * 4.f * 4.f;
    drawText(window, name, (W - tw) * 0.5f, y0 - 26.f, 4.f,
             sf::Color(245, 230, 150));
}

void Game::render() {
    window.clear(sf::Color(24, 30, 26));

    window.setView(view);
    drawWorld();

    drawUI();

    window.display();
}

void Game::run() {
    sf::Clock clock;
    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();
        if (dt > 0.10f) dt = 0.10f;

        handleEvents();
        update(dt);
        render();
    }
}

// ----------------------------------------------------------------------------
int main() {
    Game game;
    game.run();
    return 0;
}