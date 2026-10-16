#include "data.hpp"
#include "../third_party/json.hpp"
#include "io.hpp"
using nlohmann::json;

static bool readJson(const std::string& p, json& j, std::string& err) {
    std::string text;
    if (!readFileBytes(p, text)) { err = "нет файла " + p; return false; }
    try { j = json::parse(text); } catch (std::exception& e) { err = p + ": " + e.what(); return false; }
    return true;
}
static std::map<std::string, int> readCost(const json& j) {
    std::map<std::string, int> m;
    if (j.contains("cost")) for (auto& kv : j.at("cost").items()) m[kv.key()] = kv.value().get<int>();
    return m;
}
bool GameData::load(const std::string& dir, std::string& err) {
    json j;
    if (!readJson(dir + "/buildings.json", j, err)) return false;
    for (auto& b : j["buildings"]) {
        BuildingDef d; d.id = b["id"]; d.name = b["name"]; d.era = b.value("era", 1); d.w = b.value("w", 1); d.h = b.value("h", 1);
        d.housing = b.value("housing", 0); d.storage = b.value("storage", 0); d.hp = b.value("hp", 200);
        d.job = b.value("job", ""); d.needsSkill = b.value("needs_skill", "");
        d.cost = readCost(b);
        if (b.contains("trains")) for (auto& t : b["trains"]) d.trains.push_back(t);
        if (b.contains("color")) for (int i = 0; i < 3; i++) d.color[i] = b["color"][i];
        buildings.push_back(d);
    }
    if (!readJson(dir + "/units.json", j, err)) return false;
    for (auto& u : j["units"]) {
        UnitDef d; d.id = u["id"]; d.name = u["name"]; d.hp = u.value("hp", 1);
        d.speed = u.value("speed", 0.0f); d.dmg = u.value("dmg", 0.0f); d.range = u.value("range", 1.0f); d.reload = u.value("reload", 1.0f);
        d.splash = u.value("splash", 0.0f); d.minRange = u.value("min_range", 0.0f); d.vsBuildings = u.value("vs_buildings", 1.0f);
        d.weapon = u.value("weapon", ""); d.needsSkill = u.value("needs_skill", ""); d.buildIn = u.value("build_in", "");
        d.cost = readCost(u);
        units.push_back(d);
    }
    if (!readJson(dir + "/skills.json", j, err)) return false;
    pointsPerEra = j.value("points_per_era", 2);
    for (auto& s : j["skills"]) {
        SkillDef d; d.id = s["id"]; d.branch = s["branch"]; d.name = s["name"]; d.desc = s.value("desc", ""); d.tier = s.value("tier", 1); d.cost = s.value("cost", 1);
        if (s.contains("requires")) for (auto& r : s["requires"]) d.requires.push_back(r);
        if (s.contains("unlocks")) { d.unlockBuilding = s["unlocks"].value("building", ""); d.unlockUnit = s["unlocks"].value("unit", ""); }
        skills.push_back(d);
    }
    if (!readJson(dir + "/eras.json", j, err)) return false;
    for (auto& e : j["eras"]) eras.push_back({e["id"], e["name"], e.value("pop", 10)});
    if (!readJson(dir + "/chapters.json", j, err)) return false;
    for (auto& c : j["chapters"]) {
        ChapterDef d; d.id = c.value("id", 0); d.title = c.value("title", ""); d.intro = c.value("intro", ""); d.attackEra = c.value("attack_era", 3);
        if (c.contains("camps")) for (auto& cj : c["camps"]) {
            CampDef k; k.x = cj.value("x", 0); k.y = cj.value("y", 0); k.hp = cj.value("hp", 400.0f);
            if (cj.contains("garrison")) for (auto& kv : cj["garrison"].items()) k.garrison.push_back({kv.key(), kv.value().get<int>()});
            d.camps.push_back(k);
        }
        if (c.contains("waves")) for (auto& w : c["waves"]) {
            WaveDef wd; wd.at = w.value("at", 0.0f);
            for (auto& kv : w["units"].items()) wd.units.push_back({kv.key(), kv.value().get<int>()});
            d.waves.push_back(wd);
        }
        chapters.push_back(d);
    }
    return true;
}
const BuildingDef* GameData::building(const std::string& id) const { for (auto& b : buildings) if (b.id == id) return &b; return nullptr; }
const UnitDef* GameData::unit(const std::string& id) const { for (auto& u : units) if (u.id == id) return &u; return nullptr; }
const SkillDef* GameData::skill(const std::string& id) const { for (auto& s : skills) if (s.id == id) return &s; return nullptr; }
