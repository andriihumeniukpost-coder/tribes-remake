#pragma once
#include "data.hpp"
#include "gfx.hpp"
#include "input.hpp"
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

enum class Mode { Play, Build, Skills, Pause, Over };
enum Tile : unsigned char { GRASS, FOREST, ROCK, WATER };
struct Placed { const BuildingDef* def = nullptr; int x = 0, y = 0; float hp = 0; bool alive = true; };
struct Villager { float x = 0, y = 0; int tx = -1, ty = -1; int carry = 0, carryType = 0; float timer = 0; int state = 0; float hp = 30; };
struct Soldier { const UnitDef* def = nullptr; float x = 0, y = 0, hp = 1, gx = 0, gy = 0; int faction = 0; float cd = 0; };  // faction 0 — племя, 1 — враги
struct Fx { float x, y, t, r; Color c; };

class Game {
public:
    static constexpr int MW = 40, MH = 30, TS = 32;
    bool init(const std::string& dataDir);
    void debugUnlock();
    void update(const Input& in, float dt);
    void draw(Gfx& g);
    bool quit = false;
    std::string stats() const;
private:
    GameData data;
    Tile map[MW][MH];
    int occ[MW][MH];
    std::vector<Placed> buildings;
    std::vector<Villager> villagers;
    std::vector<Soldier> soldiers;
    std::vector<Fx> fxs;
    std::map<std::string, int> res{{"wood", 40}, {"stone", 20}, {"food", 30}};
    std::set<std::string> learned;
    int era = 1, skillPoints = 0, pop = 0, spentPoints = 0;
    int cx = 20, cy = 15, camX = 0, camY = 0;
    Mode mode = Mode::Play;
    int menuSel = 0, skillCol = 0, skillRow = 0;
    float eraTimer = 0, foodTimer = 0, gameTime = 0;
    size_t nextWave = 0; int wavesSpawned = 0;
    std::string toast; float toastT = 0;
    void say(const std::string& s) { toast = s; toastT = 2.5f; }
    std::vector<const BuildingDef*> buildable() const;
    bool canAfford(const std::map<std::string, int>& cost) const;
    void pay(const std::map<std::string, int>& cost);
    bool tryPlace(const BuildingDef& b);
    bool tryTrain();
    bool canLearn(const SkillDef& s) const;
    bool tryAdvanceEra();
    const SkillDef* skillAt(int col, int row) const;
    bool hasBuilding(const std::string& id) const;
    int housing() const;
    int buildingAt(int tx, int ty) const;
    int addBuilding(const BuildingDef& b, int x, int y);
    void spawnVillager();
    void spawnSoldier(const UnitDef& u, float x, float y, int faction);
    void spawnWave(const WaveDef& w);
    void updateVillagers(float dt);
    void updateSoldiers(float dt);
    void moveTo(Soldier& s, float tx, float ty, float dt, float stop);
    void damage(int kind, int idx, float amount);
    void cleanup();
    int soldierCount(int faction) const;
    void drawMap(Gfx& g); void drawHud(Gfx& g); void drawBuildMenu(Gfx& g); void drawSkills(Gfx& g); void drawHints(Gfx& g);
};
