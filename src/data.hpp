#pragma once
#include <map>
#include <string>
#include <utility>
#include <vector>
// Все правила игры живут в JSON (data/*.json): здания, юниты, умения, эпохи, волны врагов.
struct BuildingDef { std::string id, name, job, needsSkill; int era = 1, w = 1, h = 1, housing = 0, storage = 0, hp = 200;
    std::map<std::string, int> cost; std::vector<std::string> trains; unsigned char color[3] = {128, 128, 128}; };
struct UnitDef { std::string id, name, weapon, needsSkill, buildIn; int hp = 1;
    float speed = 0, dmg = 0, range = 1, reload = 1, splash = 0, minRange = 0, vsBuildings = 1; std::map<std::string, int> cost; };
struct SkillDef { std::string id, branch, name, desc, unlockBuilding, unlockUnit; int tier = 1, cost = 1; std::vector<std::string> requires; };
struct EraDef { int id; std::string name; int pop; };
struct ChapterDef { int id, startEra; std::string title, intro; };
struct WaveDef { float at = 0; std::vector<std::pair<std::string, int>> units; };
struct GameData {
    std::vector<BuildingDef> buildings; std::vector<UnitDef> units; std::vector<SkillDef> skills;
    std::vector<EraDef> eras; std::vector<ChapterDef> chapters; std::vector<WaveDef> waves;
    int pointsPerEra = 2;
    bool load(const std::string& dir, std::string& err);
    const BuildingDef* building(const std::string& id) const;
    const UnitDef* unit(const std::string& id) const;
    const SkillDef* skill(const std::string& id) const;
};
